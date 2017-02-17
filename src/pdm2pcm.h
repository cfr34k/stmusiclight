#ifndef PDM2PCM_H
#define PDM2PCM_H

#include <stdint.h>
#include <stdlib.h>

#define FILTER_ORDER 4

struct fifo_ctx;

struct pdm2pcm_ctx {
	uint32_t downsample_ratio;
	uint32_t downsample_step;

	int32_t x[FILTER_ORDER+1];
	int32_t y[FILTER_ORDER+1];
};

void pdm2pcm_init(struct pdm2pcm_ctx *ctx, uint32_t oversampling_ratio);
uint8_t pdm2pcm_update(struct pdm2pcm_ctx *ctx, uint32_t data_in, int32_t *sample_out);

void pdm2pcm_decode_to_fifo(struct pdm2pcm_ctx *ctx, uint32_t *data, size_t datalen,
                            struct fifo_ctx *fifo);

#endif // PDM2PCM_H
