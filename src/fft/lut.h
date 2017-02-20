#ifndef LUT_H
#define LUT_H
// This file was auto-generated using gen_lut.py

#include "config.h"

#define LUT_SIZE 128

static fft_value_type cos_lut[LUT_SIZE] = {1.0, 0.9996988187, 0.9987954562, 0.9972904567, 0.9951847267, 0.9924795346, 0.9891765100, 0.9852776424, 0.9807852804, 0.9757021300, 0.9700312532, 0.9637760658, 0.9569403357, 0.9495281806, 0.9415440652, 0.9329927988, 0.9238795325, 0.9142097557, 0.9039892931, 0.8932243012, 0.8819212643, 0.8700869911, 0.8577286100, 0.8448535652, 0.8314696123, 0.8175848132, 0.8032075315, 0.7883464276, 0.7730104534, 0.7572088465, 0.7409511254, 0.7242470830, 0.7071067812, 0.6895405447, 0.6715589548, 0.6531728430, 0.6343932842, 0.6152315906, 0.5956993045, 0.5758081914, 0.5555702330, 0.5349976199, 0.5141027442, 0.4928981922, 0.4713967368, 0.4496113297, 0.4275550934, 0.4052413140, 0.3826834324, 0.3598950365, 0.3368898534, 0.3136817404, 0.2902846773, 0.2667127575, 0.2429801799, 0.2191012402, 0.1950903220, 0.1709618888, 0.1467304745, 0.1224106752, 0.0980171403, 0.0735645636, 0.0490676743, 0.0245412285, 0.0000000000, -0.0245412285, -0.0490676743, -0.0735645636, -0.0980171403, -0.1224106752, -0.1467304745, -0.1709618888, -0.1950903220, -0.2191012402, -0.2429801799, -0.2667127575, -0.2902846773, -0.3136817404, -0.3368898534, -0.3598950365, -0.3826834324, -0.4052413140, -0.4275550934, -0.4496113297, -0.4713967368, -0.4928981922, -0.5141027442, -0.5349976199, -0.5555702330, -0.5758081914, -0.5956993045, -0.6152315906, -0.6343932842, -0.6531728430, -0.6715589548, -0.6895405447, -0.7071067812, -0.7242470830, -0.7409511254, -0.7572088465, -0.7730104534, -0.7883464276, -0.8032075315, -0.8175848132, -0.8314696123, -0.8448535652, -0.8577286100, -0.8700869911, -0.8819212643, -0.8932243012, -0.9039892931, -0.9142097557, -0.9238795325, -0.9329927988, -0.9415440652, -0.9495281806, -0.9569403357, -0.9637760658, -0.9700312532, -0.9757021300, -0.9807852804, -0.9852776424, -0.9891765100, -0.9924795346, -0.9951847267, -0.9972904567, -0.9987954562, -0.9996988187};



static inline fft_value_type lookup_sin(int layer, int element) {
	int idx = element * (1 << (FFT_EXPONENT - layer - 1)) + LUT_SIZE/2;
	int factor = 1;
	if(idx >= LUT_SIZE) {
		idx &= LUT_SIZE-1;
		factor = -1;
	}
	return factor * cos_lut[idx];
}

static inline fft_value_type lookup_cos(int layer, int element) {
	int idx = element * (1 << (FFT_EXPONENT - layer - 1));
	return cos_lut[idx];
}

#endif // LUT_H
