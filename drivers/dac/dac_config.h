/**
 * @file dac_config.h
 * @brief DAC Driver Configuration
 * 
 * Configuration parameters specific to the DAC (MCP4725) driver.
 */

#ifndef DAC_CONFIG_H
#define DAC_CONFIG_H

/*==============================================================================
 * DAC HARDWARE CONFIGURATION
 *============================================================================*/

/** @brief TWIM (I2C) instance index for DAC */
#define DAC_TWIM_INST_IDX 0

/** @brief I2C SDA pin for DAC (MCP4725) */
#define DAC_SDA_PIN 26

/** @brief I2C SCL pin for DAC (MCP4725) */
#define DAC_SCL_PIN 27

/** @brief MCP4725 DAC I2C address (0x60 or 0x61) */
#define DAC_I2C_ADDR 0x60

#endif /* DAC_CONFIG_H */
