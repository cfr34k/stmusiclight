#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>

/* 
 * Internal initialization for the debugging module.
 * This does NOT initialize the hardware!
 */
void debug_init( void );

void debug_send_string(char *str);

#endif // DEBUG_H
