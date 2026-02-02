/**
 * @file timer.h
 * @brief Timer driver abstraction for pulse generation and state machine
 * 
 * This module manages two timers:
 * - TIMER_PULSE: Generates precise pulse waveforms via GPPI
 * - TIMER_STATE: Controls state machine with dual CC channels for MUX pre-loading
 */

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include <nrfx_timer.h>

/**
 * @brief Initialize both pulse and state timers
 * 
 * Configures timers with 32-bit width and appropriate base frequencies.
 * Sets up pulse timer compare channels for waveform generation.
 * Registers state timer handler for state machine callbacks.
 * 
 * @param pulse_width_us Initial pulse width in microseconds
 * @return NRFX_SUCCESS on success, error code otherwise
 */
nrfx_err_t timer_init(uint32_t pulse_width_us);

/**
 * @brief Update pulse timer with new pulse width
 * 
 * Disables pulse timer, reconfigures all compare channels for new timing,
 * and re-enables timer. Safe to call from interrupt context.
 * 
 * @param pulse_width_us New pulse width in microseconds
 */
void timer_update_pulse_width(uint32_t pulse_width_us);

/**
 * @brief Configure state timer for active pulse state
 * 
 * Sets up dual CC channels:
 * - CC0: State transition at single_pulse_us
 * - CC1: MUX pre-load event (advance_time before CC0)
 * 
 * @param single_pulse_us Duration of single pulse cycle
 */
void timer_set_state_pulse(uint32_t single_pulse_us);

/**
 * @brief Configure state timer for pause state
 * 
 * Sets up dual CC channels for pause period:
 * - CC0: State transition after pause completes
 * - CC1: MUX pre-load event for next pulse
 * 
 * @param pause_us Duration of pause period in microseconds
 */
void timer_set_state_pause(uint32_t pause_us);

/**
 * @brief Enable/disable pulse timer
 * 
 * @param enable true to enable, false to disable
 */
void timer_pulse_enable(bool enable);

/**
 * @brief Get timer instances for external use
 * 
 * @param[out] pulse Pointer to receive pulse timer instance
 * @param[out] state Pointer to receive state timer instance
 */
void timer_get_instances(nrfx_timer_t **pulse, nrfx_timer_t **state);

/**
 * @brief Get state transition counter
 * 
 * @return Number of state transitions since startup
 */
uint32_t timer_get_transition_count(void);

/**
 * @brief Stop the entire pulse generation system
 * 
 * Disables both pulse and state timers, sets MUX to off pattern.
 * System can be restarted with timer_system_start().
 */
void timer_system_stop(void);

/**
 * @brief Start/restart the pulse generation system
 * 
 * Re-enables timers and restarts from STATE_PULSE_1.
 * Must be called after timer_init() has been executed.
 */
void timer_system_start(void);

/**
 * @brief Check if system is currently running
 * 
 * @return true if system is running, false if stopped
 */
bool timer_system_is_running(void);

#endif // TIMER_H
