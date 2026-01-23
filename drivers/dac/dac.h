/**
 * @file dac.h
 * @brief MCP4725 DAC Driver Interface
 * 
 * I2C-based DAC driver using nrfx_twim (no device tree overlay required)
 */

#ifndef DAC_H
#define DAC_H

#include <stdint.h>
#include <nrfx_twim.h>

/**
 * @brief Initialize DAC (MCP4725) via I2C
 * @return NRFX_SUCCESS on success, error code otherwise
 */
nrfx_err_t dac_init(void);

/**
 * @brief Set DAC output value (10-bit resolution)
 * @param value DAC value (0-1023), automatically clamped to valid range
 */
void dac_set_value(uint16_t value);

/**
 * @brief Get current DAC configuration state
 * @return true if DAC is initialized and ready
 */
bool dac_is_ready(void);

/**
 * @brief Scan I2C bus for connected devices
 */
void dac_i2c_scan(void);

/**
 * @brief Test I2C signal generation
 */
void dac_i2c_signal_test(void);

/**
 * @brief Display I2C pin configuration
 */
void dac_i2c_pin_test(void);

#endif // DAC_H
