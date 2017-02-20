#!/usr/bin/env python3
# vim: noexpandtab ts=4 sw=4 sts=4

import sys
from math import *

preamble = """#ifndef LUT_H
#define LUT_H
// This file was auto-generated using gen_lut.py

#include "config.h"

"""

postamble = """

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
"""

if len(sys.argv) < 2:
	print("Argument required: FFT_EXPONENT")
	exit(1)

fft_exponent = int(sys.argv[1])

with open("src/fft/lut.h", "w") as ofile:
	ofile.write(preamble)

	# generate the cos() lookup table
	num_elements = (1 << (fft_exponent-1))
	ofile.write("#define LUT_SIZE {:d}\n\n".format(num_elements))

	ofile.write("static fft_value_type cos_lut[LUT_SIZE] = {")

	ofile.write("1.0")

	for element in range(1, num_elements):
		ofile.write(", %.10f" % cos(-pi * element / num_elements));

	ofile.write("};\n\n")

	ofile.write(postamble)
