/**
 * @file gpiote.h
 * @brief GPIOTE driver for output pin control with task mode
 * 
 * This module manages GPIOTE configuration for hardware-triggered output pins.
 * Provides task-based control for pulse generation without CPU intervention.
 */

#ifndef GPIOTE_H
#define GPIOTE_H

#include <stdint.h>
#include <nrfx.h>

/**
 * @brief Initialize GPIOTE module and allocate channels
 * 
 * Initializes GPIOTE instance and allocates 2 channels for OUTPUT_PIN_1 and OUTPUT_PIN_2.
 * Configures both pins as task-controlled outputs with initial HIGH state.
 * 
 * @param[out] ch_pin1 Pointer to store allocated channel for PIN1
 * @param[out] ch_pin2 Pointer to store allocated channel for PIN2
 * 
 * @return NRFX_SUCCESS on success, error code otherwise
 */
nrfx_err_t gpiote_init(uint8_t *ch_pin1, uint8_t *ch_pin2);

#endif // GPIOTE_H
