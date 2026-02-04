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

/** @brief GPIO pin for output channel 1 (defaults to LED2 - pulse toggle) */
#define OUTPUT_PIN_1 LED2_PIN

/** @brief GPIO pin for output channel 2 (defaults to LED1 - constant high during sequence) */
#define OUTPUT_PIN_2 LED1_PIN

#endif /* GPIOTE_CONFIG_H */
