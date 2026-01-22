/**
 * @file mux.h
 * @brief Multiplexer driver interface for SPI-based channel control
 *
 * Copyright (c) 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MUX_H
#define MUX_H

#include <stdint.h>
#include <nrfx_spim.h>

#ifdef __cplusplus
extern "C" {
#endif

/* MUX GPIO Control Pins */
#define MUX_LE_PIN           1     /* P0.1  - Latch Enable */
#define MUX_CLR_PIN          0     /* P0.0  - Clear */
#define MUX_NUM_CHANNELS     16    /* 16 channels via 2-byte shift register */

/**
 * @brief Initialize MUX hardware (GPIO + SPIM)
 * 
 * Configures GPIO pins for LE and CLR control, initializes SPIM peripheral,
 * and clears all MUX channels.
 * 
 * @param spim Pointer to SPIM instance to use
 * @return NRFX_SUCCESS on success, error code otherwise
 */
nrfx_err_t mux_init(nrfx_spim_t *spim);

/**
 * @brief Write 16-bit data to MUX via SPI and latch
 * 
 * Sends 16-bit data over SPI (MSB first) and pulses LE pin to latch
 * the data from shift register to output.
 * 
 * @param data 16-bit channel data (bit position = channel number)
 * @return NRFX_SUCCESS on success, error code otherwise
 */
nrfx_err_t mux_write(uint16_t data);

/**
 * @brief Check if MUX transfer is complete
 * 
 * @return true if no transfer in progress, false otherwise
 */
bool mux_is_ready(void);

/**
 * @brief Wait for MUX transfer to complete (blocking)
 * Only use this outside of ISR context!
 */
void mux_wait_ready(void);

#ifdef __cplusplus
}
#endif

#endif // MUX_H
