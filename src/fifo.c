#include "fifo.h"

extern inline void fifo_init(volatile struct fifo_ctx *ctx);
extern inline void fifo_push(volatile struct fifo_ctx *ctx, fifo_t value);
extern inline fifo_t fifo_pop(volatile struct fifo_ctx *ctx);
extern inline uint8_t fifo_is_empty(volatile struct fifo_ctx *ctx);
extern inline uint8_t fifo_is_full(volatile struct fifo_ctx *ctx);
extern inline uint16_t fifo_get_level(volatile struct fifo_ctx *ctx);
