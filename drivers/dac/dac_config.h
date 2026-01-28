/**
 * @file dac_config.h
 * @brief DAC Driver Configuration
 * 
 * Configuration parameters specific to the SPI DAC driver.
 */

#ifndef DAC_CONFIG_H
#define DAC_CONFIG_H

/*==============================================================================
 * DAC HARDWARE CONFIGURATION
 *============================================================================*/

/** @brief SPIM instance index for DAC (independent from MUX) */
#define DAC_SPIM_INST_IDX 2

#define DAC_CS_PIN   16   // ← Slobodan pin
#define DAC_MOSI_PIN 3   // ← Slobodan pin
#define DAC_SCK_PIN  4   // ← Slobodan pin



/** 
 * @brief DAC resolution (depends on your SPI DAC chip)
 * Common values: 8, 10, 12, 16 bits
 */
#define DAC_RESOLUTION_BITS 12

/** @brief Maximum DAC value (2^resolution - 1) */
#define DAC_MAX_VALUE ((1 << DAC_RESOLUTION_BITS) - 1)

#endif /* DAC_CONFIG_H */