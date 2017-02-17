#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>

#include "debug.h"


#define DEBUG_BUFFER_SIZE 256
volatile char debugBuffer[DEBUG_BUFFER_SIZE];

volatile uint16_t readPos, writePos;
volatile uint8_t bufferFull, bufferEmpty, usartActive;

void debug_init()
{
  readPos = 0;
  writePos = 0;
  bufferFull = 0;
  bufferEmpty = 1;
  usartActive = 0;

  // enable USART2 clock
  rcc_peripheral_enable_clock(&RCC_APB1ENR, RCC_APB1ENR_USART2EN);

  // enable GPIO clocks:
  // Port A is needed for USART2
	rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_IOPAEN);

  // Set up USART2
  gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO2);
  gpio_set_af(GPIOA, GPIO_AF7, GPIO2);

  // enable interrupt
  nvic_enable_irq(NVIC_USART2_IRQ);

  /* Setup USART2 parameters. */
  usart_set_baudrate(USART2, 115200);
  usart_set_databits(USART2, 8);
  usart_set_stopbits(USART2, USART_STOPBITS_1);
  usart_set_mode(USART2, USART_MODE_TX);
  usart_set_parity(USART2, USART_PARITY_NONE);
  usart_set_flow_control(USART2, USART_FLOWCONTROL_NONE);

  /* Enable the USART2 TX Interrupt */
  usart_enable_tx_interrupt(USART2);

  /* Finally enable the USART. */
  usart_enable(USART2);
}


static void debug_transfer(void)
{
  if(bufferEmpty) {
    usart_disable_tx_interrupt(USART2);
    usartActive = 0;
    return;
  }

  usart_send(USART2, debugBuffer[readPos]);

  readPos++;
  if(readPos == DEBUG_BUFFER_SIZE) {
    readPos = 0;
  }

  if(readPos == writePos) {
    bufferEmpty = 1;
  }

  bufferFull = 0;
  usart_enable_tx_interrupt(USART2);
}


void usart2_isr(void)
{
  if(USART2_SR & USART_SR_TXE){
    debug_transfer();
  }
}


void debug_send_string(char *str)
{
  while(*str) {
    if(bufferFull) {
      break;
    }

    debugBuffer[writePos] = *str;

    writePos++;
    if(writePos == DEBUG_BUFFER_SIZE) {
      writePos = 0;
    }

    if(writePos == readPos) {
      bufferFull = 1;
    }

    bufferEmpty = 0;

    str++;
  }

  if(!usartActive) {
    usartActive = 1;
    debug_transfer();
  }
}
