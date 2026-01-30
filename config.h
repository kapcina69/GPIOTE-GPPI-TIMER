/**
 * @file config.h
 * @brief System Configuration File
 * 
 * This file contains general project-wide configuration parameters.
 * Driver-specific configurations are in their respective config files:
 * - drivers/mux/mux_config.h - MUX driver configuration
 * - drivers/saadc/saadc_config.h - ADC configuration
 * - drivers/timers/timer_config.h - Timer configuration
 * - drivers/gpiote/gpiote_config.h - GPIOTE configuration
 * - drivers/dac/dac_config.h - DAC configuration
 * 
 * @note All timing values are carefully tuned for the nRF52833 running at 64MHz
 */

#ifndef CONFIG_H
#define CONFIG_H

/* Include driver-specific configurations */
#include "drivers/mux/mux_config.h"
#include "drivers/saadc/saadc_config.h"
#include "drivers/timers/timer_config.h"
#include "drivers/gpiote/gpiote_config.h"
#include "drivers/dac/dac_config.h"

/*==============================================================================
 * PULSE GENERATION PARAMETERS
 *============================================================================*/

/**
 * @defgroup pulse_params Pulse Generation Parameters
 * @brief Constants for pulse timing calculations
 * @{
 */

/** 
 * @brief Number of sequential pulses per cycle
 * @note The system generates 8 pulses in sequence, then enters PAUSE state.
 *       Each pulse routes through a different MUX channel.
 */
#define NUM_PULSES_PER_CYCLE 8

/** 
 * @brief Overhead time added to each pulse (microseconds)
 * @note This accounts for timer setup, GPPI propagation delays, and MUX switching.
 *       Formula: single_pulse_duration = (pulse_width_ms * 100) * 2 + PULSE_OVERHEAD_US
 *       
 *       Breakdown:
 *       - pulse_width_ms * 100: Convert from 100µs units to µs
 *       - * 2: Pulse consists of HIGH + LOW periods
 *       - + PULSE_OVERHEAD_US: Fixed overhead for each pulse
 */
#define PULSE_OVERHEAD_US 100

/** 
 * @brief Multiplier for pulse width calculation
 * @note Pulse width from BLE is in units of 100µs (e.g., pulse_width=5 means 500µs).
 *       This multiplier converts to microseconds.
 */
#define PULSE_WIDTH_MULTIPLIER 100

/**
 * @brief Calculate total active time for one complete pulse cycle
 * @param pulse_width_100us Pulse width in units of 100µs (from BLE command)
 * @return Total active time in microseconds for 8 pulses
 * 
 * Formula: ACTIVE_TIME = [(pulse_width * 100) * 2 + 100] * 8
 * Example: pulse_width=5 (500µs) → [(5*100)*2+100]*8 = [1000+100]*8 = 8800µs
 */
#define CALCULATE_ACTIVE_TIME_US(pulse_width_100us) \
    (((pulse_width_100us) * PULSE_WIDTH_MULTIPLIER * 2 + PULSE_OVERHEAD_US) * NUM_PULSES_PER_CYCLE)

/**
 * @brief Calculate maximum allowed frequency for a given pulse width
 * @param pulse_width_100us Pulse width in units of 100µs
 * @return Maximum frequency in Hz (ensures PAUSE period is non-negative)
 * 
 * Formula: max_freq = 1000000µs / ACTIVE_TIME
 * This ensures: PERIOD >= ACTIVE_TIME, so PAUSE = PERIOD - ACTIVE_TIME >= 0
 */
#define CALCULATE_MAX_FREQUENCY_HZ(pulse_width_100us) \
    (1000000U / CALCULATE_ACTIVE_TIME_US(pulse_width_100us))

/** @} */ // end of pulse_params

/*==============================================================================
 * FEATURE ENABLES
 *============================================================================*/

/**
 * @defgroup features Feature Flags
 * @brief Enable/disable optional features for testing and power optimization
 * @{
 */

/** 
 * @brief Enable periodic statistics printing
 * @note When enabled, prints sample count and state transitions every second.
 *       Useful for debugging but adds ~500µA current consumption.
 *       
 *       Set to 0 for production (minimal power consumption).
 *       Set to 1 for development/debugging.
 */
#define ENABLE_STATS_TIMER 0

/** 
 * @brief Enable ADC sample logging
 * @note When enabled, logs ADC samples to console (respecting LOG_EVERY_N_SAMPLES).
 *       Useful for verifying ADC operation but increases power consumption.
 *       
 *       Set to 0 to disable all ADC logging (lowest power).
 *       Set to 1 to enable periodic ADC logging.
 */
#define ENABLE_ADC_LOGGING 1

/** @} */ // end of features

/**
 * @brief Enable DAC pre-loading with per-pulse values
 *
 * When enabled (1), the timer state handler will call `dac_set_value()`
 * for the next pulse during the MUX pre-load event (CC1). Default is 0
 * (disabled) to avoid extra SPI activity unless explicitly requested.
 */
#define ENABLE_DAC_PRELOAD 1
/*==============================================================================
 * LOGGING CONFIGURATION
 *============================================================================*/

/**
 * @defgroup logging Logging Configuration
 * @brief Console output and debug logging settings
 * @{
 */

/** @brief NRFX logging module name */
#define NRFX_LOG_MODULE EXAMPLE

/** @brief Enable NRFX logging (0=disabled, 1=enabled) */
#define NRFX_EXAMPLE_CONFIG_LOG_ENABLED 1

/** 
 * @brief NRFX log level
 * @note Levels: 0=Off, 1=Error, 2=Warning, 3=Info, 4=Debug
 */
#define NRFX_EXAMPLE_CONFIG_LOG_LEVEL 3

/** @} */ // end of logging

#endif /* CONFIG_H */
