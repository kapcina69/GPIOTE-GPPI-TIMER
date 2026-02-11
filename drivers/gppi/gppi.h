/**
 * @file gppi.h
 * @brief GPPI (General Purpose Programmable Interconnect) driver for pulse generation
 * 
 * This module manages GPPI connections between:
 * - Timer events → GPIOTE tasks (pulse generation)
 * - Timer events → SAADC tasks (ADC triggering)
 * - Timer events → SPIM START task (hardware MUX and DAC transfer trigger)
 * - SAADC events → Timer tasks (timestamp capture)
 */

#ifndef GPPI_H
#define GPPI_H

#include <stdint.h>
#include <nrfx.h>

/**
 * @brief Initialize and allocate all GPPI channels
 * 
 * Allocates GPPI channels for:
 * - 2 channels for PIN1 (set/clear)
 * - 1 channel for ADC trigger
 * - 1 channel for ADC timestamp capture
 * - 1 channel for MUX SPIM start trigger
 * - 1 channel for DAC SPIM start trigger
 * 
 * @return NRFX_SUCCESS on success, error code otherwise
 */
nrfx_err_t gppi_init(void);

/**
 * @brief Setup GPPI connections between timer, GPIOTE, and SAADC
 * 
 * Connects:
 * - Timer CC0 → PIN1 clear (pulse start)
 * - Timer CC1 → PIN1 set (pulse end)
 * - Timer CC0 → SAADC sample trigger
 * - State timer CC1 → SPIM START (send preloaded MUX pattern)
 * - State timer CC1 → SPIM START (send preloaded DAC value)
 * - SAADC END → Timer capture (timestamp)
 * 
 * @param gpiote_ch_pin1 GPIOTE channel for PIN1
 * @param gpiote_ch_pin2 GPIOTE channel for PIN2
 * 
 * @return NRFX_SUCCESS on success, error code otherwise
 */
nrfx_err_t gppi_setup_connections(uint8_t gpiote_ch_pin1, uint8_t gpiote_ch_pin2);

/**
 * @brief Enable all GPPI channels
 * 
 * Activates all 6 GPPI channels simultaneously.
 */
void gppi_enable(void);

#endif // GPPI_H
