#ifndef PDM2PCM_H
#define PDM2PCM_H

#include <stdint.h>
#include <stdlib.h>

#define FILTER_ORDER 4

struct fifo_ctx;

struct pdm2pcm_ctx {
	uint32_t downsample_ratio;
	uint32_t downsample_step;

	int32_t cic_accu1;
	int32_t cic_accu2;

	int32_t cic_comb1_d1;
	int32_t cic_comb1_d2;

	int32_t cic_comb2_d1;
	int32_t cic_comb2_d2;
};

void pdm2pcm_init(struct pdm2pcm_ctx *ctx, uint32_t oversampling_ratio);
uint8_t pdm2pcm_update(struct pdm2pcm_ctx *ctx, uint32_t data_in, int32_t *sample_out);

void pdm2pcm_decode_to_fifo(struct pdm2pcm_ctx *ctx, uint32_t *data, size_t datalen,
                            struct fifo_ctx *fifo);

#endif // PDM2PCM_H
