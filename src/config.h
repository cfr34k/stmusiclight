#ifndef CONFIG_H
#define CONFIG_H

#define FFT_EXPONENT     8

// length of the transformed block (input and output length)
#define FFT_BLOCK_LEN    (1 << FFT_EXPONENT)

// length of the useful FFT result data (due to symmetry)
#define FFT_DATALEN      (FFT_BLOCK_LEN/2 + 1)

#define SAMPLE_RATE      40000

typedef float fft_value_type;
typedef float fft_sample;

#endif // CONFIG_H
