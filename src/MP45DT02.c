#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/dma.h>

#include "MP45DT02.h"

void mp45dt02_init(uint32_t *buf0, uint32_t *buf1, uint32_t n)
{
#if 1
	// This init code is also in the CS43L22 module. If that module is not used,
	// enable this block.

	// enable I2S PLL
	RCC_CFGR &= ~RCC_CFGR_I2SSRC; // use PLLI2S as I2S clock

	/* VCO frequency: 1 MHz
	 * PLLI2S output: VCO * N / R = 86 MHz
	 */
	RCC_PLLI2SCFGR = (3 << RCC_PLLI2SCFGR_PLLI2SR_SHIFT)
		| (258 << RCC_PLLI2SCFGR_PLLI2SN_SHIFT);

	RCC_CR |= RCC_CR_PLLI2SON; // start PLLI2S

	while((RCC_CR & RCC_CR_PLLI2SRDY) == 0); // wait until PLLI2S is ready
#endif

	// clock setup
	rcc_peripheral_enable_clock(&RCC_APB1ENR, RCC_APB1ENR_SPI2EN);

	rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_IOPBEN); // for I2S2 Data
	rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_IOPCEN); // for I2S2 Clock

	// reset the peripheral
	spi_reset(SPI2);

	// set up GPIO
	// SD
	gpio_mode_setup(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO3);
	gpio_set_af(GPIOC, GPIO_AF5, GPIO3);

	// SCK
	gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO10);
	gpio_set_af(GPIOB, GPIO_AF5, GPIO10);

	// configure SPI2 as I2S receiver
	// bit clock = 3.071428 MHz; Sample rate (64 ticks per sample) = 47.991 kHz
	SPI2_I2SPR |= /*SPI_I2SPR_MCKOE | SPI_I2SPR_ODD |*/ (14 << 0); // ~MCKOE, ~ODD, I2SDIV = 14
	SPI2_I2SCFGR |= SPI_I2SCFGR_CKPOL; // Clock high when idle
	SPI2_I2SCFGR |= SPI_I2SCFGR_I2SMOD
		| (SPI_I2SCFGR_I2SSTD_MSB_JUSTIFIED << SPI_I2SCFGR_I2SSTD_LSB)
		| (SPI_I2SCFGR_DATLEN_32BIT << SPI_I2SCFGR_DATLEN_LSB)
		| SPI_I2SCFGR_CHLEN // 32 bit channel length
		| (SPI_I2SCFGR_I2SCFG_MASTER_RECEIVE << SPI_I2SCFGR_I2SCFG_LSB);

	/*** Setup DMA ***/

	// clock setup
	rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_DMA1EN);

	// Setup DMA for I2S2:
	// stream 3, channel 0, 16 transfers burst from memory, single transfers to peripheral
	// double buffer mode, 32bit input, 16bit output, increase memory addresse, copy from periph to mem
	DMA1_S3CR = DMA_SxCR_CHSEL_0 | DMA_SxCR_MBURST_SINGLE | DMA_SxCR_PBURST_SINGLE | DMA_SxCR_PL_HIGH
		| DMA_SxCR_DBM | DMA_SxCR_MSIZE_32BIT | DMA_SxCR_PSIZE_16BIT | DMA_SxCR_MINC | DMA_SxCR_DIR_PERIPHERAL_TO_MEM;

	DMA1_S3PAR = &SPI2_DR;
	DMA1_S3M0AR = buf0;
	DMA1_S3M1AR = buf1;
	DMA1_S3FCR = DMA_SxFCR_DMDIS | DMA_SxFCR_FTH_2_4_FULL;
	DMA1_S3NDTR = 2*n;

	SPI2_CR2 |= SPI_CR2_RXDMAEN;
}

void mp45dt02_start(void)
{
	// Enable the DMA
	DMA1_S3CR |= DMA_SxCR_EN;

	// start the I2S module
	SPI2_I2SCFGR |= SPI_I2SCFGR_I2SE;
}

int mp45dt02_dma_get_current_buffer( void )
{
	return (DMA1_S3CR & DMA_SxCR_CT) != 0;
}
