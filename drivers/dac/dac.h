/**
 * @file dac.h
 * @brief SPI DAC Driver Interface
 * 
 * SPI-based DAC driver using nrfx_spim (no device tree overlay required)
 */

#ifndef DAC_H
#define DAC_H

#include <stdint.h>
#include <nrfx_spim.h>

/**
 * @brief Initialize DAC via SPI
 * @param spim Pointer to SPIM instance to use
 * @return NRFX_SUCCESS on success, error code otherwise
 */
nrfx_err_t dac_init(nrfx_spim_t *spim);

/**
 * @brief Set DAC output value (async, non-blocking)
 * @param value DAC value (0 to DAC_MAX_VALUE), automatically clamped
 * @return NRFX_SUCCESS if transfer started, NRFX_ERROR_BUSY if previous transfer ongoing
 */
nrfx_err_t dac_set_value(uint16_t value);

/**
 * @brief Check if DAC is ready for new transfer
 * @return true if ready, false if transfer in progress
 */
bool dac_is_ready(void);

/**
 * @brief Wait for DAC transfer to complete (blocking)
 * Only use outside ISR context!
 */
void dac_wait_ready(void);

#endif // DAC_H