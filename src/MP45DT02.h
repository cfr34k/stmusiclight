#ifndef MP45DT02_H
#define MP45DT02_H

#include <stdint.h>

void mp45dt02_init(uint32_t *buf0, uint32_t *buf1, uint32_t n);
void mp45dt02_start(void);

int  mp45dt02_dma_get_current_buffer(void);

#endif // MP45DT02_H
