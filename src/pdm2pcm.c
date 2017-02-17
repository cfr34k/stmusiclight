#include "pdm2pcm.h"

#include "fifo.h"

/*
 * Implementation idea:
 * - low-pass filtering and decimation of incoming bits (samples)
 * - using a moving average
 */


// source: Hacker’s Delight, page 66
// https://books.google.de/books?id=iBNKMspIlqEC&pg=PA66&redir_esc=y#v=onepage&q&f=false
static inline uint32_t count_bits(uint32_t x)
{
	x = x - ((x >> 1) & 0x55555555);
	x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
	x = (x + (x >> 4)) & 0x0F0F0F0F;
	x = x + (x >> 8);
	x = x + (x >> 16);
	return x & 0x0000003F;
}

void pdm2pcm_init(struct pdm2pcm_ctx *ctx, uint32_t oversampling_ratio)
{
	ctx->downsample_ratio = (oversampling_ratio + 16) / 32;
	ctx->downsample_step = 0;

	for(int i = 0; i <= FILTER_ORDER; i++) {
		ctx->x[i] = ctx->y[i] = 0;
	}
}

uint8_t pdm2pcm_update(struct pdm2pcm_ctx *ctx, uint32_t data_in, int32_t *sample_out)
{
	uint8_t sample_generated = 0;

	/*
	// 18 kHz LPF
	const int32_t denom[FILTER_ORDER+1] = {16383, -29479, 32171, -18862, 5409};
	const int32_t numer[FILTER_ORDER+1] = {313, 1252, 1879, 1252, 313};
	*/

	// 15 kHz LPF
	const int32_t denom[FILTER_ORDER+1] = {16383, -37382, 42172, -24792, 6481};
	const int32_t numer[FILTER_ORDER+1] = {159,  638,  957,  638, 159};

	for(int i = FILTER_ORDER; i>0; i--) {
		ctx->x[i] = ctx->x[i-1];
		ctx->y[i] = ctx->y[i-1];
	}

	ctx->x[0] = (((int32_t)count_bits(data_in) - 16) << 11);

	ctx->y[0] = numer[0] * ctx->x[0];

	for(int i = FILTER_ORDER; i>0; i--) {
		ctx->y[0] += ctx->x[i] * numer[i] - ctx->y[i] * denom[i];
	}

	ctx->y[0] >>= 14;

	ctx->downsample_step++;
	if(ctx->downsample_step >= ctx->downsample_ratio) {
		ctx->downsample_step = 0;

		*sample_out = ctx->y[0];
		sample_generated = 1;
	}

	return sample_generated;
}

void pdm2pcm_decode_to_fifo(struct pdm2pcm_ctx *ctx, uint32_t *data, size_t datalen,
                            struct fifo_ctx *fifo)
{
	int32_t sample;

	for(size_t w = 0; w < datalen; w++) {
		uint32_t word = data[w];

		if(pdm2pcm_update(ctx, word, &sample)) {
			fifo_push(fifo, sample << 7);
		}
	}
}
