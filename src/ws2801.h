#ifndef WS2801_H
#define WS2801_H

#include <stdint.h>

#define WS2801_NUM_MODULES 32

void ws2801_init(void);
void ws2801_setup_dma(void);
void ws2801_set_colour(uint8_t module, float red, float green, float blue);
void ws2801_send_update(void);

#endif // WS2801_H
