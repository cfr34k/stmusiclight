/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2009 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2011 Stephen Caudle <scaudle@doceme.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "debug.h"
#include "tictoc.h"
#include "ws2801.h"
#include "MP45DT02.h"
#include "pdm2pcm.h"
#include "fifo.h"
#include "constants.h"

#define FPS 100

#define AUDIO_BUFFER_SIZE 48
#define SAMPLE_BUFFER_SIZE 256

volatile uint8_t tick_ms = 1;

static void init_gpio(void)
{
	// Set up LED outputs for PWM
	gpio_mode_setup(GPIOD, GPIO_MODE_AF, GPIO_PUPD_NONE,
			GPIO12 | GPIO13 | GPIO14 | GPIO15);

	gpio_set_af(GPIOD, 2, GPIO12 | GPIO13 | GPIO14 | GPIO15);
}

static void init_clock(void)
{
	/* Set STM32 to 120 MHz. */
	rcc_clock_setup_hse_3v3(&rcc_hse_8mhz_3v3[RCC_CLOCK_3V3_120MHZ]);

	// enable GPIO clocks:
	// Port D is needed for LEDs
	rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_IOPDEN);

	// enable TIM1 clock
	rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_TIM1EN);

	// Set up USART2
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO2);
	gpio_set_af(GPIOA, GPIO_AF7, GPIO2);

	RCC_APB2ENR |= RCC_APB2ENR_TIM1EN;

	// enable TIM4 clock
	rcc_peripheral_enable_clock(&RCC_APB1ENR, RCC_APB1ENR_TIM4EN);
}

static void init_timer(void)
{
	// global interrupt config
	nvic_enable_irq(NVIC_TIM1_UP_TIM10_IRQ);

	// *** TIM1 ***

	// - upcounter
	// - clock: CK_INT
	// - only overflow generates update interrupt
	TIM1_CR1 |= TIM_CR1_URS;

	// defaults for TIM_CR2

	// enable update interrupt
	TIM1_DIER |= TIM_DIER_UIE;

	// prescaler
	TIM1_PSC = 119; // count up by 1 every 1 us

	// auto-reload (maximum value)
	TIM1_ARR = 999; // overflow every 1 ms

	// 48 kHz interrupt frequency
	// TIM1_PSC = 24; // count up by 1 every 208.33 ns
	// TIM1_ARR = 99; // multiply interval by 100 -> 20.833 us

	// GO!
	TIM1_CR1 |= TIM_CR1_CEN;

	// *** TIM4 ***
	timer_reset(TIM4);
	timer_set_mode(TIM4, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);

	// set up PWM channels
	timer_set_oc_mode(TIM4, TIM_OC1, TIM_OCM_PWM1);
	timer_set_oc_mode(TIM4, TIM_OC2, TIM_OCM_PWM1);
	timer_set_oc_mode(TIM4, TIM_OC3, TIM_OCM_PWM1);
	timer_set_oc_mode(TIM4, TIM_OC4, TIM_OCM_PWM1);
	timer_enable_oc_output(TIM4, TIM_OC1);
	timer_enable_oc_output(TIM4, TIM_OC2);
	timer_enable_oc_output(TIM4, TIM_OC3);
	timer_enable_oc_output(TIM4, TIM_OC4);
	timer_enable_oc_preload(TIM4, TIM_OC1);
	timer_enable_oc_preload(TIM4, TIM_OC2);
	timer_enable_oc_preload(TIM4, TIM_OC3);
	timer_enable_oc_preload(TIM4, TIM_OC4);

	// prescaler
	timer_set_prescaler(TIM4, 120); // count up by 1 every 1 us

	// auto-reload value
	timer_set_period(TIM4, 999); // 1000 Hz PWM

	// GO!
	timer_enable_counter(TIM4);
}

static void sinusfader(uint32_t tick_count)
{
	for(uint8_t i = 0; i < WS2801_NUM_MODULES; i++) {
		float bphase = 2*PI*((float)i/WS2801_NUM_MODULES + tick_count/5000.0f);

		ws2801_set_colour(i,
				0.5f + 0.5f * sinf(bphase + 0*PI/4),
				0.5f + 0.5f * sinf(bphase + 2*PI/3),
				0.5f + 0.5f * sinf(bphase + 4*PI/3));
	}

	ws2801_send_update();
}

static void musiclight_mono(uint32_t tick_count, float *samples)
{
	static float v[WS2801_NUM_MODULES];

	static float maxrms = 1;

	float avg = 0;
	float rms = 0;

	(void)tick_count; // avoid unused parameter warning

	// step 1: calculate average
	for(uint32_t i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
		avg += samples[i];
	}

	avg /= SAMPLE_BUFFER_SIZE;

	// step 2: calculate average
	for(uint32_t i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
		float tmp = samples[i] - avg;
		rms += tmp*tmp;
	}

	rms /= SAMPLE_BUFFER_SIZE;

	// step 3: shift the value history
	for(uint8_t i = WS2801_NUM_MODULES-1; i > 0; i--) {
		v[i] = v[i-1];
	}

	// step 4: calculate new value
	maxrms *= 0.999f;
	if(rms > maxrms) {
		maxrms = rms;
	}

	v[0] = rms / maxrms;

	// step 5: assign values to LEDs
	for(uint8_t i = 0; i < WS2801_NUM_MODULES; i++) {
		ws2801_set_colour(i, v[i], v[i], v[i]);
	}

	ws2801_send_update();
}

int main(void)
{
	uint32_t tick_count = 0;

	uint32_t mic_buf0[AUDIO_BUFFER_SIZE], mic_buf1[AUDIO_BUFFER_SIZE];
	uint32_t *cur_mic_buf = mic_buf0;
	uint32_t *old_mic_buf = 0;

	float sample_buffer[SAMPLE_BUFFER_SIZE];

	struct fifo_ctx sample_fifo;
	struct pdm2pcm_ctx pdmctx;

	init_clock();
	init_gpio();
	init_timer();

	debug_init();
	tictoc_init();

	mp45dt02_init(mic_buf0, mic_buf1, AUDIO_BUFFER_SIZE);

	fifo_init(&sample_fifo);
	pdm2pcm_init(&pdmctx, 128); // downsampling factor -> 24 kHz

	ws2801_init();
	ws2801_setup_dma();

	debug_send_string("Init complete\r\n");

	mp45dt02_start();

	while (1) {
		cur_mic_buf = mp45dt02_dma_get_current_buffer() ? mic_buf0 : mic_buf1;

		if( old_mic_buf != cur_mic_buf){
			pdm2pcm_decode_to_fifo(&pdmctx, cur_mic_buf, AUDIO_BUFFER_SIZE, &sample_fifo);

			if(fifo_get_level(&sample_fifo) >= SAMPLE_BUFFER_SIZE) {
				for(uint32_t i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
					sample_buffer[i] = fifo_pop(&sample_fifo);
				}
			}

			old_mic_buf = cur_mic_buf;
		}

		if(tick_ms == 1) {
			tick_ms = 0;
			tick_count++;

			if((tick_count % (1000 / FPS)) == 0) {
				//sinusfader(tick_count);
				musiclight_mono(tick_count, sample_buffer);
			}
		}
	}

	return 0;
}

void tim1_up_tim10_isr(void)
{
	// check for update interrupt
	if(TIM1_SR & TIM_SR_UIF) {
		tick_ms = 1;
		TIM1_SR &= ~(TIM_SR_UIF); // clear interrupt flag

	}
}

void hard_fault_handler(void)
{
	while(1);
}

void usage_fault_handler(void)
{
	while(1);
}