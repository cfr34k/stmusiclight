#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>

#include "tictoc.h"

static uint32_t tictoc_last_value;

void tictoc_init(void)
{
  // enable TIM4 clock
	rcc_peripheral_enable_clock(&RCC_APB1ENR, RCC_APB1ENR_TIM6EN);

  // - upcounter
  // - clock: CK_INT
  TIM6_CR1 = 0x0000;

  // defaults for TIM_CR2

  // prescaler
  TIM6_PSC = 59; // 60 MHz APB1 clock -> 1 MHz counter frequency

  // auto-reload (maximum value)
  TIM6_ARR = 0xFFFFFFFF; // overflow every 1 ms

  // generate an update event
  TIM6_EGR |= TIM_EGR_UG;

  // start the timer
  TIM6_CR1 |= TIM_CR1_CEN;
}

void tic(void)
{
  timer_set_counter(TIM6, 0);
}

void toc(void)
{
  tictoc_last_value = timer_get_counter(TIM6);
}

uint32_t tictoc_get_last_duration(void)
{
  return tictoc_last_value;
}
