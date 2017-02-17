#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/spi.h>

#include "ws2801.h"

/*
 * IMPORTANT:
 *
 * This module uses inverted logic, as simple transistors are used for level
 * conversion. This inverts the signal, so we have to correct this in software.
 */

void send_message(void);
void dma_stream5_isr(void);

static uint8_t message[3*WS2801_NUM_MODULES];

void send_message(void)
{
  // wait for previous DMA request to complete
  while(DMA1_S5CR & DMA_SxCR_EN);

  // switch MOSI to SPI mode
  gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO3 | GPIO5);

  // initiate DMA transfer
  DMA1_S5M0AR = message;
  DMA1_S5NDTR = 3*WS2801_NUM_MODULES;
  DMA1_HIFCR |= DMA_HIFCR_CTCIF5 | DMA_HIFCR_CDMEIF5 | DMA_HIFCR_CFEIF5 | DMA_HIFCR_CTEIF5;
  DMA1_S5CR |= DMA_SxCR_EN;
}

void dma_stream5_isr(void)
{
  if(DMA1_HISR & DMA_HISR_TCIF5) {
    // force MOSI low after transmission is complete (for data commit)
    gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO3 | GPIO5);
    gpio_clear(GPIOB, GPIO3 | GPIO5);

    // clear interrupt flag
    DMA1_HIFCR |= DMA_HIFCR_CTCIF5;
  }
}

void ws2801_setup_dma(void)
{
  // clock setup
	rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_DMA1EN);

  // Setup DMA for I2S3:
  // stream 5, channel 0, 16 transfers burst from memory, single transfers to peripheral
  // 8bit input, 8bit output, increase memory address, copy from mem to periph,
  // enable transfer complete interrupt
  DMA1_S5CR = DMA_SxCR_CHSEL_0 | DMA_SxCR_MBURST_SINGLE |
    DMA_SxCR_PBURST_SINGLE | DMA_SxCR_PL_MEDIUM | DMA_SxCR_MSIZE_8BIT |
    DMA_SxCR_PSIZE_8BIT | DMA_SxCR_MINC | DMA_SxCR_DIR_MEM_TO_PERIPHERAL |
    DMA_SxCR_TCIE;

  DMA1_S5PAR = &SPI3_DR;
  DMA1_S5FCR = DMA_SxFCR_DMDIS | DMA_SxFCR_FTH_2_4_FULL;

  spi_enable_tx_dma(SPI3);
}

void ws2801_init(void)
{
  // *** set up SPI3
  // enable clocks
	rcc_peripheral_enable_clock(&RCC_APB1ENR, RCC_APB1ENR_SPI3EN);
	rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_IOPBEN); // for SPI

  // enable MOSI and SCK pins for SPI (MISO and CS are not needed for WS2801)
  //gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO3 /*| GPIO4*/ | GPIO5);
  gpio_set_af(GPIOB, GPIO_AF6, GPIO3 /*| GPIO4*/ | GPIO5);

  // GPIOs are normal outputs if no SPI transfer is running
  gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO3 | GPIO5);
  gpio_set(GPIOB, GPIO3 | GPIO5);

  // SPI configuration
  spi_reset(SPI3);
  // TODO: check this
  spi_init_master(SPI3, SPI_CR1_BAUDRATE_FPCLK_DIV_64,
      SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE, SPI_CR1_CPHA_CLK_TRANSITION_1,    // normal polarity
      //SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE, SPI_CR1_CPHA_CLK_TRANSITION_2,  // inverse polarity
      SPI_CR1_DFF_8BIT, SPI_CR1_MSBFIRST);

  // software chip select (not used for WS2801)
  spi_enable_software_slave_management(SPI3);
  spi_set_nss_high(SPI3);

  spi_enable(SPI3);
}

void ws2801_set_colour(uint8_t module, float red, float green, float blue)
{
  // perform gamma correction and convert to integer
  uint8_t red_int   = 255.0f * red*red;
  uint8_t green_int = 255.0f * green*green;
  uint8_t blue_int  = 255.0f * blue *blue;

  // Invert signal, as we have inverting level shifters
	//message[3*module + 0] = ~red_int;
	//message[3*module + 1] = ~green_int;
	//message[3*module + 2] = ~blue_int;

	message[3*module + 0] = red_int;
	message[3*module + 1] = green_int;
	message[3*module + 2] = blue_int;
}

void ws2801_send_update(void)
{
	send_message();
}

