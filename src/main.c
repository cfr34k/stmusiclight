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
#include <libopencm3/stm32/adc.h>
#include <libopencm3/cm3/nvic.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include "debug.h"
#include "tictoc.h"
#include "ws2801.h"
#include "MP45DT02.h"
#include "pdm2pcm.h"
#include "fifo.h"
#include "fft/fft.h"
#include "constants.h"

#define FPS 100

#define AUDIO_BUFFER_SIZE 48
#define SAMPLE_BUFFER_SIZE 256

volatile uint8_t tick_ms = 1;

volatile struct fifo_ctx sample_fifo;


static void init_gpio(void)
{
	// Set up LED outputs for PWM
	gpio_mode_setup(GPIOD, GPIO_MODE_AF, GPIO_PUPD_NONE,
			GPIO12 | GPIO13 | GPIO14 | GPIO15);

	gpio_set_af(GPIOD, 2, GPIO12 | GPIO13 | GPIO14 | GPIO15);

	// Set up analog input
	gpio_mode_setup(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO4);
}

static void init_clock(void)
{
	/* Set STM32 to 120 MHz. */
	rcc_clock_setup_hse_3v3(&rcc_hse_8mhz_3v3[RCC_CLOCK_3V3_120MHZ]);

	// enable GPIO clocks:
	// Port D is needed for LEDs
	rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_IOPDEN);

	// Port A is needed for ADC
	rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_IOPAEN);

	// enable TIM1 clock
	rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_TIM1EN);

	// Set up USART2
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO2);
	gpio_set_af(GPIOA, GPIO_AF7, GPIO2);

	RCC_APB2ENR |= RCC_APB2ENR_TIM1EN;

	// enable TIM4 clock
	rcc_peripheral_enable_clock(&RCC_APB1ENR, RCC_APB1ENR_TIM4EN);

	// enable TIM3 clock
	rcc_peripheral_enable_clock(&RCC_APB1ENR, RCC_APB1ENR_TIM3EN);

  // enable ADC1 clock
	//rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_ADC1EN);
	rcc_periph_clock_enable(RCC_ADC1);
}

static void init_timer(void)
{
	// global interrupt config
	nvic_enable_irq(NVIC_TIM1_UP_TIM10_IRQ);
	nvic_enable_irq(NVIC_ADC_IRQ);

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

	// *** TIM3 ***
	timer_reset(TIM3);
	timer_set_mode(TIM3, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);

	// prescaler
	timer_set_prescaler(TIM3, 120); // count up by 1 every 1 us

	// auto-reload value
	timer_set_period(TIM3, 25); // 40 kHz tick rate

	timer_set_master_mode(TIM3, TIM_CR2_MMS_UPDATE);

	// GO!
	timer_enable_counter(TIM3);
}

static void init_adc(void)
{
	uint8_t channel = ADC_CHANNEL4;

	adc_set_multi_mode(ADC_CCR_MULTI_INDEPENDENT);

	adc_power_off(ADC1);

	adc_disable_scan_mode(ADC1);
	adc_set_single_conversion_mode(ADC1);
	adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_144CYC);
	adc_set_right_aligned(ADC1);
	adc_set_regular_sequence(ADC1, 1, &channel);

	adc_enable_external_trigger_regular(ADC1, ADC_CR2_EXTSEL_TIM3_TRGO, ADC_CR2_EXTEN_RISING_EDGE);
	adc_enable_eoc_interrupt(ADC1);

	adc_power_on(ADC1);

	///* Wait for ADC starting up. */
	//int i;
	//for (i = 0; i < 800000; i++) /* Wait a bit. */
	//	__asm__("nop");

	//adc_reset_calibration(ADC1);
	//adc_calibration(ADC1);
	//adc_calibrate(ADC1);
}

static bool sinusfader(uint32_t tick_count, float *samples)
{
	(void)samples; // avoid unused parameter warning

	for(uint8_t i = 0; i < WS2801_NUM_MODULES; i++) {
		float bphase = 2*PI*((float)i/WS2801_NUM_MODULES + tick_count/5000.0f);

		ws2801_set_colour(i,
				0.5f + 0.5f * sinf(bphase + 0*PI/4),
				0.5f + 0.5f * sinf(bphase + 2*PI/3),
				0.5f + 0.5f * sinf(bphase + 4*PI/3));
	}

	ws2801_send_update();

	return false;
}

static bool musiclight_mono(uint32_t tick_count, float *samples)
{
	static float v[WS2801_NUM_MODULES];

	static float maxrms = 1e-10;

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

	return false;
}

enum MusiclightStep {
	MS_WINDOW,
	MS_FFT,
	MS_FFT_ABS,
	MS_FFT_DENOISE,
	MS_EXTRACT_ENERGY,
	MS_UPDATE_COLORS,
	MS_APPLY
};

#define MUSICLIGHT_COOLDOWN_FACTOR 0.9998f
#define MUSICLIGHT_NOISE_COOLDOWN_FACTOR 0.99999f
#define MUSICLIGHT_HEATUP_FACTOR 1.0002f
#define MUSICLIGHT_OVERDRIVE 1.0f

//#define COMMONMAX

static bool musiclight(uint32_t tick_count, float *samples)
{
	static float r[WS2801_NUM_MODULES];
	static float g[WS2801_NUM_MODULES];
	static float b[WS2801_NUM_MODULES];

	static float energy_r = 0;
	static float energy_g = 0;
	static float energy_b = 0;

#ifndef COMMONMAX
	static float max_r = 1e-30f;
	static float max_g = 1e-30f;
	static float max_b = 1e-30f;
#else
	static float total_energy = 0;
	static float max_total_energy = 0.001f;
#endif

	/*
	static float min_r = 1e30f;
	static float min_g = 1e30f;
	static float min_b = 1e30f;
	*/

	static fft_sample local_samples[FFT_BLOCK_LEN];
	static fft_value_type fft_re[FFT_BLOCK_LEN];
	static fft_value_type fft_im[FFT_BLOCK_LEN];
	static fft_value_type fft_abs[FFT_DATALEN];

	static fft_value_type fft_abs_avg[FFT_DATALEN];

	const fft_value_type fft_avg_alpha = 0.00001f;

	static enum MusiclightStep cur_step = MS_WINDOW;

	// reset 1 second after startup
	if(tick_count <= 1000) {
		cur_step = MS_WINDOW;

		for(uint32_t i = 0; i < FFT_DATALEN; i++) {
			fft_abs_avg[i] = 0.0f;
		}

#ifndef COMMONMAX
		max_r = 1e-30f;
		max_g = 1e-30f;
		max_b = 1e-30f;
#else
		total_energy = 0;
		max_total_energy = 0.001f;
#endif

		for(uint8_t i = 0; i < WS2801_NUM_MODULES; i++) {
			ws2801_set_colour(i, 0, 0, 0);
		}

		ws2801_send_update();

		return false;
	}

	switch(cur_step) {
		case MS_WINDOW:
			fft_copy_windowed(samples, local_samples);
			cur_step = MS_FFT;
			return true;
			break;

		case MS_FFT:
			fft_transform(samples, fft_re, fft_im);
			cur_step = MS_FFT_ABS;
			return true;
			break;

		case MS_FFT_ABS:
			fft_complex_to_absolute(fft_re, fft_im, fft_abs);
			cur_step = MS_FFT_DENOISE;
			return true;
			break;

		case MS_FFT_DENOISE:
			// calculate a long-term average for the power in each FFT bin using an
			// exponential averaging filter. This should mostly contain the noise
			// power, as the signal will probably vary over time.
			for(int i = 0; i < FFT_DATALEN; i++) {
				//fft_abs_avg[i] *= MUSICLIGHT_NOISE_COOLDOWN_FACTOR;
				//if(fft_abs[i] > fft_abs_avg[i]) {
					fft_abs_avg[i] = fft_avg_alpha * fft_abs[i] + (1 - fft_avg_alpha) * fft_abs_avg[i];
				//}

				fft_abs[i] -= fft_abs_avg[i];
				if(fft_abs[i] < 0) {
					fft_abs[i] = 0;
				}
			}

			cur_step = MS_EXTRACT_ENERGY;
			return true;
			break;

		case MS_EXTRACT_ENERGY:
#ifdef COMMONMAX
			energy_r = fft_get_energy_in_band(fft_abs, 80, 400) / 400;
			energy_g = fft_get_energy_in_band(fft_abs, 400, 4000) / 3600;
			energy_b = fft_get_energy_in_band(fft_abs, 4000, 10000) / 6000;
#else
			// ignore feedback frequency from the LED stripe (2,5 kHz) and overtones
			energy_r  = fft_get_energy_in_band(fft_abs, 0, 400);
			energy_g  = fft_get_energy_in_band(fft_abs, 400, 2450);
			energy_g += fft_get_energy_in_band(fft_abs, 2550, 4000);
			energy_b  = fft_get_energy_in_band(fft_abs, 4000, 4935);
			energy_b += fft_get_energy_in_band(fft_abs, 5065, 7420);
			energy_b += fft_get_energy_in_band(fft_abs, 7580, 9900);
#endif

#ifdef COMMONMAX
			total_energy = energy_r + energy_g + energy_b;
#endif

			cur_step = MS_UPDATE_COLORS;
			return true;
			break;

		case MS_UPDATE_COLORS:
#ifndef COMMONMAX
			max_r *= MUSICLIGHT_COOLDOWN_FACTOR;
			max_g *= MUSICLIGHT_COOLDOWN_FACTOR;
			max_b *= MUSICLIGHT_COOLDOWN_FACTOR;

			if(energy_r > max_r) { max_r = energy_r; }
			if(energy_g > max_g) { max_g = energy_g; }
			if(energy_b > max_b) { max_b = energy_b; }
#else
			max_total_energy *= MUSICLIGHT_COOLDOWN_FACTOR;

			if(total_energy > max_total_energy) { max_total_energy = total_energy; }
#endif

			/*
			min_r *= MUSICLIGHT_HEATUP_FACTOR;
			min_g *= MUSICLIGHT_HEATUP_FACTOR;
			min_b *= MUSICLIGHT_HEATUP_FACTOR;

			if(energy_r < min_r) { min_r = energy_r; }
			if(energy_g < min_g) { min_g = energy_g; }
			if(energy_b < min_b) { min_b = energy_b; }
			*/

			for(uint8_t i = WS2801_NUM_MODULES-1; i > 1; i-=2) {
				r[i] = r[i-2];
				g[i] = g[i-2];
				b[i] = b[i-2];
				r[i-1] = r[i-3];
				g[i-1] = g[i-3];
				b[i-1] = b[i-3];
			}

#ifndef COMMONMAX
			//r[0] = (energy_r - min_r) / (max_r - min_r);
			//g[0] = (energy_g - min_g) / (max_g - min_g);
			//b[0] = (energy_b - min_b) / (max_b - min_b);
			r[1] = r[0] = energy_r / max_r;
			g[1] = g[0] = energy_g / max_g;
			b[1] = b[0] = energy_b / max_b;
#else
			r[0] = MUSICLIGHT_OVERDRIVE * energy_r / max_total_energy;
			g[0] = MUSICLIGHT_OVERDRIVE * energy_g / max_total_energy;
			b[0] = MUSICLIGHT_OVERDRIVE * energy_b / max_total_energy;

			if(r[0] > 1.0f) { r[0] = 1.0f; };
			if(g[0] > 1.0f) { g[0] = 1.0f; };
			if(b[0] > 1.0f) { b[0] = 1.0f; };
#endif

			cur_step = MS_APPLY;
			return true;
			break;

		case MS_APPLY:
			for(uint8_t i = 0; i < WS2801_NUM_MODULES; i++) {
				ws2801_set_colour(i, r[i], g[i], b[i]);
			}

			ws2801_send_update();

			cur_step = MS_WINDOW;
			return false;
			break;

	};

	return false;
}

int main(void)
{
	uint32_t tick_count = 0;

	//uint32_t mic_buf0[AUDIO_BUFFER_SIZE], mic_buf1[AUDIO_BUFFER_SIZE];
	//uint32_t *cur_mic_buf = mic_buf0;
	//uint32_t *old_mic_buf = 0;

	float sample_buffer[SAMPLE_BUFFER_SIZE];

	bool must_update = false;

	init_clock();
	init_gpio();
	init_adc();
	init_timer();

	debug_init();
	tictoc_init();

	fifo_init(&sample_fifo);

	ws2801_init();
	ws2801_setup_dma();

	fft_init();

	debug_send_string("Init complete\r\n");

	timer_set_oc_value(TIM4, TIM_OC1, 100);

	while (1) {
		/*
		cur_mic_buf = mp45dt02_dma_get_current_buffer() ? mic_buf0 : mic_buf1;

		if( old_mic_buf != cur_mic_buf){
			pdm2pcm_decode_to_fifo(&pdmctx, cur_mic_buf, AUDIO_BUFFER_SIZE, &sample_fifo);

			if(fifo_get_level(&sample_fifo) >= SAMPLE_BUFFER_SIZE) {
				for(uint32_t i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
					sample_buffer[i] = fifo_pop(&sample_fifo) / (float)(1 << 30);
				}

				// new samples arrived
				must_update = true;
			}

			old_mic_buf = cur_mic_buf;
		}
		*/

		if(fifo_get_level(&sample_fifo) >= SAMPLE_BUFFER_SIZE) {
			for(uint32_t i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
				nvic_disable_irq(NVIC_ADC_IRQ); // start critical section
				sample_buffer[i] = fifo_pop(&sample_fifo) / (float)(1 << 12);
				nvic_enable_irq(NVIC_ADC_IRQ); // end critical section
			}

			// new samples arrived
			must_update = true;
		}

		if(must_update) {
			must_update = musiclight(tick_count, sample_buffer);
		}

		if(tick_ms == 1) {
			tick_ms = 0;
			tick_count++;
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

#define ADC_LOWPASS_EXPONENT 18

void adc_isr(void)
{
	static uint32_t adcavg = 2048;

	uint16_t adcval;

	if(adc_eoc(ADC1)) { //ADC1_SR & ADC_SR_EOC) {
		adcval = adc_read_regular(ADC1);

		adcavg = (adcavg - (adcavg >> ADC_LOWPASS_EXPONENT)) + adcval;

		fifo_push(&sample_fifo, (fifo_t)adcval - (adcavg >> ADC_LOWPASS_EXPONENT));
		//timer_set_oc_value(TIM4, TIM_OC2, adcval >> 2);
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
