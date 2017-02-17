#ifndef TICTOC_H
#define TICTOC_H

#include <stdint.h>

/*!
 * Set up a timer for time measurements.
 */
void tictoc_init(void);

/*!
 * Start a measurement.
 */
void tic(void);

/*!
 * Stop the measurement.
 *
 * The result can be retrieved using tictoc_get_last_duration().
 */
void toc(void);

/*!
 * Get the last duration value measured.
 *
 * \returns The duration in timer ticks.
 */
uint32_t tictoc_get_last_duration(void);

#endif // TICTOC_H
