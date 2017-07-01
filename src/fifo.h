#ifndef FIFO_H
#define FIFO_H

#include <stdint.h>

#include "config.h"

#define FIFO_DEPTH (FFT_BLOCK_LEN*2)

typedef int32_t fifo_t;

struct fifo_ctx {
	fifo_t data[FIFO_DEPTH];

	uint16_t widx, ridx;
	uint16_t level;
};

/* implemented in header file for possible inlining */

#define FIFO_INC(x) ((x + 1) % FIFO_DEPTH)

inline void fifo_init(volatile struct fifo_ctx *ctx)
{
	ctx->widx = 0;
	ctx->ridx = 0;
	ctx->level = 0;
}

inline void fifo_push(volatile struct fifo_ctx *ctx, fifo_t value)
{
	uint16_t tmpidx = FIFO_INC(ctx->widx);

	if(tmpidx != ctx->ridx) {
		ctx->data[ctx->widx] = value;
		ctx->widx = tmpidx;
		ctx->level++;
	}
}

inline fifo_t fifo_pop(volatile struct fifo_ctx *ctx)
{
	fifo_t value = 0;

	if(ctx->ridx != ctx->widx) {
		value = ctx->data[ctx->ridx];
		ctx->ridx = FIFO_INC(ctx->ridx);
		ctx->level--;
	}

	return value;
}

inline uint8_t fifo_is_empty(volatile struct fifo_ctx *ctx)
{
	if(ctx->widx == ctx->ridx) {
		return 1;
	} else {
		return 0;
	}
}

inline uint8_t fifo_is_full(volatile struct fifo_ctx *ctx)
{
	if(FIFO_INC(ctx->widx) == ctx->ridx) {
		return 1;
	} else {
		return 0;
	}
}

inline uint16_t fifo_get_level(volatile struct fifo_ctx *ctx)
{
	return ctx->level;
}

#endif // FIFO_H
