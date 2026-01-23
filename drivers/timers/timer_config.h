/**
 * @file timer_config.h
 * @brief Timer Driver Configuration
 * 
 * Configuration parameters specific to the Timer drivers.
 */

#ifndef TIMER_CONFIG_H
#define TIMER_CONFIG_H

#include <nrfx_timer.h>

/*==============================================================================
 * TIMER HARDWARE CONFIGURATION
 *============================================================================*/

/** @brief Timer instance for pulse generation (TIMER1) */
#define TIMER_PULSE_IDX 1

/** @brief Timer instance for state machine (TIMER2) */
#define TIMER_STATE_IDX 2

/*==============================================================================
 * VALIDATION
 *============================================================================*/

#if TIMER_PULSE_IDX == TIMER_STATE_IDX
#error "TIMER_PULSE_IDX and TIMER_STATE_IDX must be different!"
#endif

#endif /* TIMER_CONFIG_H */
