/**
 * @file timer.h
 * @brief Timer driver abstraction for pulse generation and state machine
 * 
 * This module manages two timers:
 * - TIMER_PULSE: Generates precise pulse waveforms via GPPI
 * - TIMER_STATE: Controls state machine with dual CC channels for MUX pre-loading
 * 
 * NEW MODE: Supports configurable pulse count (1-16)
 * - Default: 16 pulses per cycle
 * - LED1 toggles for all pulses, LED2 stays low
 * - Can be changed via timer_set_pulse_count() or SC command
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
 * Re-enables timers and restarts from first pulse.
 * Must be called after timer_init() has been executed.
 */
void timer_system_start(void);

/**
 * @brief Check if system is currently running
 * 
 * @return true if system is running, false if stopped
 */
bool timer_system_is_running(void);

/**
 * @brief Set the number of pulses per cycle
 * 
 * Configures how many pulses are generated before PAUSE.
 * Can be used by SC command to change the pulse count dynamically.
 * 
 * @param count Number of pulses (1 to MAX_PULSES_PER_CYCLE, typically 16)
 */
void timer_set_pulse_count(uint8_t count);

/**
 * @brief Get the current pulse count setting
 * 
 * @return Current number of pulses per cycle
 */
uint8_t timer_get_pulse_count(void);

/**
 * @brief Set MUX patterns for pulse sequence
 * 
 * Called by SC command to configure which MUX outputs are activated
 * for each pulse. Patterns with value 0 indicate unused slots.
 * The number of active pulses is automatically calculated from
 * the last non-zero pattern.
 * 
 * @param patterns Array of up to 16 MUX pattern values
 * @param count Number of patterns in the array
 */
void timer_set_mux_patterns(const uint16_t *patterns, uint8_t count);

/**
 * @brief Get a specific MUX pattern
 * 
 * @param index Pattern index (0-15)
 * @return MUX pattern value, or 0 if index out of range
 */
uint16_t timer_get_mux_pattern(uint8_t index);

/**
 * @brief Set DAC values for pulse sequence
 * 
 * Called by SA command to configure DAC output for each pulse.
 * Unlike timer_set_mux_patterns(), this does NOT affect
 * the active_pulse_count.
 * 
 * @param values Array of up to 16 DAC values (0-4095)
 * @param count Number of values in the array
 */
void timer_set_dac_values(const uint16_t *values, uint8_t count);

/**
 * @brief Get a specific DAC value
 * 
 * @param index Value index (0-15)
 * @return DAC value, or 0 if index out of range
 */
uint16_t timer_get_dac_value(uint8_t index);

/**
 * @brief Get single pulse duration in microseconds
 * 
 * Calculates: (pulse_width_ms * 100) + PULSE_OVERHEAD_US
 * 
 * @return Single pulse duration in microseconds
 */
uint32_t timer_get_single_pulse_us(void);

/**
 * @brief Get total active time for all pulses in one cycle
 * 
 * Calculates: single_pulse_us * active_pulse_count
 * Takes into account the current number of active pulses (set via SC command)
 * 
 * @return Total active time in microseconds
 */
uint32_t timer_get_active_time_us(void);

/**
 * @brief Get maximum allowed frequency based on current settings
 * 
 * Calculates max frequency ensuring pause period is at least 100us.
 * Formula: max_freq = 1000000 / (active_time_us + 100)
 * 
 * @return Maximum frequency in Hz
 */
uint32_t timer_get_max_frequency_hz(void);

#endif // TIMER_H
