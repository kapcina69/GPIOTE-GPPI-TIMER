/*
 * BLE Communication Module
 * Handles BLE commands for frequency and pulse width control
 */

#ifndef BLE_H
#define BLE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize BLE module
 * 
 * @return 0 on success, negative error code on failure
 */
int ble_init(void);

/**
 * @brief Get current pause time in milliseconds (based on frequency)
 * 
 * @return Pause time in ms
 */
uint32_t ble_get_pause_time_ms(void);

/**
 * @brief Get current frequency in Hz
 * 
 * @return Frequency in Hz
 */
uint32_t ble_get_frequency_hz(void);

/**
 * @brief Get current pulse width in milliseconds
 * 
 * @return Pulse width in ms
 */
uint32_t ble_get_pulse_width_ms(void);

/**
 * @brief Get maximum allowed frequency based on pulse width
 * Ensures that PAUSE >= 0 for 8 sequential pulses
 * 
 * @param pulse_width Pulse width in units of 100µs
 * @return Maximum frequency in Hz
 */
uint32_t ble_get_max_frequency(uint32_t pulse_width);

/**
 * @brief Check if parameters have been updated since last check
 * 
 * @return true if parameters changed, false otherwise
 */
bool ble_parameters_updated(void);

/**
 * @brief Clear the updated flag after reading new parameters
 */
void ble_clear_update_flag(void);

#endif /* BLE_H */
