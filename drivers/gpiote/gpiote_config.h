/**
 * @file gpiote_config.h
 * @brief GPIOTE Driver Configuration
 * 
 * Configuration parameters specific to the GPIOTE driver.
 */

#ifndef GPIOTE_CONFIG_H
#define GPIOTE_CONFIG_H

/*==============================================================================
 * GPIOTE HARDWARE CONFIGURATION
 *============================================================================*/

/** @brief GPIOTE instance index */
#define GPIOTE_INST_IDX 0

/** @brief GPIO pin for output channel 1 (defaults to LED1) */
#define OUTPUT_PIN_1 LED1_PIN

/** @brief GPIO pin for output channel 2 (defaults to LED2) */
#define OUTPUT_PIN_2 LED2_PIN

#endif /* GPIOTE_CONFIG_H */
