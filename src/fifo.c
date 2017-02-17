#include "fifo.h"

extern inline void fifo_init(struct fifo_ctx *ctx);
extern inline void fifo_push(struct fifo_ctx *ctx, fifo_t value);
extern inline fifo_t fifo_pop(struct fifo_ctx *ctx);
extern inline uint8_t fifo_is_empty(struct fifo_ctx *ctx);
extern inline uint8_t fifo_is_full(struct fifo_ctx *ctx);
extern inline uint16_t fifo_get_level(struct fifo_ctx *ctx);
