/*
 * vim: sw=2 ts=2 expandtab
 *
 * "THE PIZZA-WARE LICENSE" (derived from "THE BEER-WARE LICENCE"):
 * Thomas Kolb <cfr34k@tkolb.de> wrote this file. As long as you retain this
 * notice you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a pizza in return.
 * - Thomas Kolb
 */

#include <math.h>
#include <stdint.h>

#include "constants.h"

#include "config.h"

#include "lut.h"
#include "fft.h"

fft_value_type window_buffer[FFT_BLOCK_LEN];
int lookup_table[FFT_BLOCK_LEN];


void fft_init(void) {
  int i = 0;
  int ri, b;

  for(i = 0; i < FFT_BLOCK_LEN; i++) {
    window_buffer[i] = 0.5 * (1 - cos(2 * PI * i / FFT_BLOCK_LEN));
  }

  for(i = 0; i < FFT_BLOCK_LEN; i++) {
    ri = 0;

    for(b = 0; b < FFT_EXPONENT; b++)
      ri |= ((i >> b) & 1) << (FFT_EXPONENT - b - 1);

    lookup_table[i] = ri;
  }
}



void fft_complex_to_absolute(fft_value_type *re, fft_value_type *im, fft_value_type *result) {
  int i;

  for(i = 0; i < FFT_DATALEN; i++)
  {
    result[i] = sqrt( re[i]*re[i] + im[i]*im[i] );
  }
}



void fft_apply_window(fft_sample *dftinput) {
  int i;

  for(i = 0; i < FFT_BLOCK_LEN; i++)
    dftinput[i] *= window_buffer[i];
}



void fft_copy_windowed(fft_sample *in, fft_sample *out) {
  int i;

  for(i = 0; i < FFT_BLOCK_LEN; i++) {
    out[i] = in[i] * window_buffer[i];
  }
}



void fft_transform(fft_sample *samples, fft_value_type *resultRe, fft_value_type *resultIm) {
  int i;
  int layer, part, element;
  int num_parts, num_elements;

  int left, right;

  fft_value_type x_left_re, x_left_im, x_right_re, x_right_im;
  fft_value_type sinval, cosval;

  // re-arrange the input array according to the lookup table
  // and store it into the real output array (as the input is obviously real).
  // zero the imaginary output
  for(i = 0; i < FFT_BLOCK_LEN; i++)
  {
    resultRe[lookup_table[i]] = samples[i];
    resultIm[i] = 0;
  }

  // walk layers
  for(layer = 0; layer < FFT_EXPONENT; layer++)
  {
    // number of parts in current layer
    num_parts = 1 << (FFT_EXPONENT - layer - 1);

    // walk parts of layer
    for(part = 0; part < num_parts; part++)
    {
      // number of elements in current part
      num_elements = (1 << layer);

      // walk elements in part
      for(element = 0; element < num_elements; element++)
      {
        // calculate index of element in left and right half of part
        left = (1 << (layer + 1)) * part + element;
        right = left + (1 << layer);

        // buffer the elements for the calculation
        x_left_re = resultRe[left];
        x_left_im = resultIm[left];
        x_right_re = resultRe[right];
        x_right_im = resultIm[right];

        // use lookup table to get sinus and cosinus values for param
        //param = -M_PI * element / (1 << layer);
        sinval = lookup_sin(layer, element);
        cosval = lookup_cos(layer, element);

        // combine the values according to a butterfly diagram
        resultRe[left]  = x_right_re + x_left_re * cosval - x_left_im * sinval;
        resultIm[left]  = x_right_im + x_left_im * cosval + x_left_re * sinval;
        resultRe[right] = x_right_re - x_left_re * cosval + x_left_im * sinval;
        resultIm[right] = x_right_im - x_left_im * cosval - x_left_re * sinval;
      }
    }
  }
}

uint32_t fft_find_loudest_frequency(fft_value_type *absFFT) {
  int maxPos = 0;
  fft_value_type maxVal = 0;
  int i;

  for(i = 0; i < FFT_BLOCK_LEN; i++) {
    if(absFFT[i] > maxVal) {
      maxPos = i;
      maxVal = absFFT[i];
    }
  }

  return (fft_value_type)maxPos * SAMPLE_RATE / FFT_BLOCK_LEN;
}

fft_value_type fft_get_energy_in_band(fft_value_type *fft, uint32_t minFreq, uint32_t maxFreq) {
  int firstBlock = minFreq * FFT_BLOCK_LEN / SAMPLE_RATE;
  int lastBlock = maxFreq * FFT_BLOCK_LEN / SAMPLE_RATE;
  int i;

  fft_value_type energy = 0;
  for(i = firstBlock; i < lastBlock; i++) {
    energy += fft[i];
  }

  return energy;
}
