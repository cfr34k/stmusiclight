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
	ctx->downsample_ratio = (oversampling_ratio + 4) / 8;
	ctx->downsample_step = ctx->downsample_ratio;

	/*
	for(int i = 0; i <= FILTER_ORDER; i++) {
		ctx->x[i] = ctx->y[i] = 0;
	}
	*/
}

uint8_t pdm2pcm_update(struct pdm2pcm_ctx *ctx, uint32_t data_in, int32_t *sample_out)
{
	uint8_t sample_generated = 0;

	// downsampling according to this article:
	// https://curiouser.cheshireeng.com/2015/01/16/pdm-in-a-tiny-cpu/
	//
	// Summary:
	// 1. sum bits in each byte using a lookup-table (CIC with R=8, M=1, N=1)
	// 2. CIC with R=downsample_ratio, M=2, N=2

	const int8_t bitsum_lut[256] = {
		#define S(n) (2*(n)-8)
		#define B2(n) S(n),  S(n+1),  S(n+1),  S(n+2)
		#define B4(n) B2(n), B2(n+1), B2(n+1), B2(n+2)
		#define B6(n) B4(n), B4(n+1), B4(n+1), B4(n+2)
		B6(0), B6(1), B6(1), B6(2)
	};

	// cache variables for faster access
	int32_t cic_accu1 = ctx->cic_accu1;
	int32_t cic_accu2 = ctx->cic_accu2;

	int32_t cic_comb1_d1 = ctx->cic_comb1_d1;
	int32_t cic_comb1_d2 = ctx->cic_comb1_d2;

	int32_t cic_comb2_d1 = ctx->cic_comb2_d1;
	int32_t cic_comb2_d2 = ctx->cic_comb2_d2;

	uint32_t downsample_step = ctx->downsample_step;

	for(uint32_t s = 0; s < 32; s+=8) {
		// first integrator
		cic_accu1 += bitsum_lut[(data_in >> s) & 0xFF];

		// second integrator
		cic_accu2 += cic_accu1;

		// downsampling
		if(--(downsample_step) == 0) {
			downsample_step = ctx->downsample_ratio;

			// first comb stage
			int32_t cic_comb1_out = cic_accu2 - cic_comb1_d2;
			cic_comb1_d2 = cic_comb1_d1;
			cic_comb1_d1 = cic_accu2;

			// second comb stage: assign output
			*sample_out = cic_comb1_out - cic_comb2_d2;
			cic_comb2_d2 = cic_comb2_d1;
			cic_comb2_d1 = cic_comb1_out;

			sample_generated = 1;
		}
	}

	// write back cached variables
	ctx->cic_accu1 = cic_accu1;
	ctx->cic_accu2 = cic_accu2;

	ctx->cic_comb1_d1 = cic_comb1_d1;
	ctx->cic_comb1_d2 = cic_comb1_d2;

	ctx->cic_comb2_d1 = cic_comb2_d1;
	ctx->cic_comb2_d2 = cic_comb2_d2;

	ctx->downsample_step = downsample_step;

	return sample_generated;
}

void pdm2pcm_decode_to_fifo(struct pdm2pcm_ctx *ctx, uint32_t *data, size_t datalen,
                            struct fifo_ctx *fifo)
{
	int32_t sample;

	for(size_t w = 0; w < datalen; w++) {
		uint32_t word = data[w];

		if(pdm2pcm_update(ctx, word, &sample)) {
			fifo_push(fifo, sample << (24-14));
		}
	}
}
