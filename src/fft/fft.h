/*
 * vim: sw=2 ts=2 expandtab
 *
 * "THE PIZZA-WARE LICENSE" (derived from "THE BEER-WARE LICENCE"):
 * Thomas Kolb <cfr34k@tkolb.de> wrote this file. As long as you retain this
 * notice you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a pizza in return.
 * - Thomas Kolb
 */

#ifndef FFT_H
#define FFT_H

#include "config.h"

void fft_init(void);
void fft_complex_to_absolute(fft_value_type *re, fft_value_type *im, fft_value_type *result);
void fft_apply_window(fft_sample *dftinput);
void fft_copy_windowed(fft_sample *in, fft_sample *out);
void fft_transform(fft_sample *samples, fft_value_type *resultRe, fft_value_type *resultIm);
uint32_t fft_find_loudest_frequency(fft_value_type *absFFT);
fft_value_type fft_get_energy_in_band(fft_value_type *fft, uint32_t minFreq, uint32_t maxFreq);

#endif // FFT_H
