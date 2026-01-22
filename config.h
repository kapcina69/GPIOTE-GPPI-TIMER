/**
 * @file config.h
 * @brief System Configuration File
 * 
 * This file contains all configurable parameters for the pulse generation system.
 * Modify these values to adjust system behavior without changing the main code.
 * 
 * @note All timing values are carefully tuned for the nRF52833 running at 64MHz
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <nrfx_timer.h>
#include <hal/nrf_saadc.h>

/*==============================================================================
 * HARDWARE CONFIGURATION
 *============================================================================*/

/**
 * @defgroup hw_config Hardware Peripheral Configuration
 * @brief Hardware resource allocation and pin assignments
 * @{
 */

/** @brief SPIM instance index for MUX control */
#define SPIM_INST_IDX 2

/** @brief Timer instance for pulse generation (TIMER1) */
#define TIMER_PULSE_IDX 1

/** @brief Timer instance for state machine (TIMER2) */
#define TIMER_STATE_IDX 2

/** @brief GPIOTE instance index */
#define GPIOTE_INST_IDX 0

/** @brief GPIO pin for output channel 1 (defaults to LED1) */
#define OUTPUT_PIN_1 LED1_PIN

/** @brief GPIO pin for output channel 2 (defaults to LED2) */
#define OUTPUT_PIN_2 LED2_PIN

/** @} */ // end of hw_config

/*==============================================================================
 * ADC CONFIGURATION
 *============================================================================*/

/**
 * @defgroup adc_config ADC Configuration
 * @brief SAADC sampling and processing parameters
 * @{
 */

/** 
 * @brief SAADC input channel selection
 * @note Use NRF_SAADC_INPUT_AIN0 through NRF_SAADC_INPUT_AIN7 for available inputs
 */
#define SAADC_CHANNEL_AIN NRF_SAADC_INPUT_AIN0

/** 
 * @brief ADC resolution (8, 10, 12, or 14 bits)
 * @note Higher resolution = more accurate but slower conversion
 */
#define SAADC_RESOLUTION NRF_SAADC_RESOLUTION_10BIT

/** 
 * @brief ADC interrupt batch size
 * @note Samples are buffered and processed in batches to reduce interrupt overhead.
 *       Higher values = less CPU overhead but higher latency.
 *       Recommended: 8 for balanced performance.
 */
#define ADC_INTERRUPT_BATCH_SIZE 8

/** 
 * @brief ADC logging frequency
 * @note Only log every Nth sample to avoid overwhelming the console.
 *       Set to 1 to log every sample (high overhead).
 *       Set to 100+ for production (minimal overhead).
 */
#define LOG_EVERY_N_SAMPLES 100

/** @} */ // end of adc_config

/*==============================================================================
 * TIMING CONFIGURATION
 *============================================================================*/

/**
 * @defgroup timing_config Timing Configuration
 * @brief Pulse generation and state machine timing parameters
 * @{
 */

/** 
 * @brief MUX pre-load advance time in microseconds
 * @note The state timer generates TWO events per state:
 *       - CC1 fires ADVANCE_TIME microseconds BEFORE state transition
 *       - CC0 fires at the actual state transition time
 *       
 *       This ensures the MUX pattern is sent early enough to arrive
 *       via SPI before the pulse starts.
 *       
 *       Typical values:
 *       - 50µs: Fast SPI, short cables
 *       - 200µs: Normal operation (recommended)
 *       - 500µs: Slow SPI or long cables
 */
#define MUX_ADVANCE_TIME_US 50

/** @} */ // end of timing_config

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
 * MUX PATTERNS
 *============================================================================*/

/**
 * @defgroup mux_patterns MUX Output Patterns
 * @brief 16-bit patterns sent to MUX for each of the 8 pulses
 * @{
 */

/** 
 * @brief MUX patterns for 8 sequential pulses
 * @note Each pattern is a 16-bit value sent to the MUX controller.
 *       Pattern format depends on your MUX hardware design.
 *       Default patterns use a walking bit pattern for demonstration.
 *       
 *       Customize these patterns based on your MUX routing requirements:
 *       - Pulse 1: Channel 0 (0x0101)
 *       - Pulse 2: Channel 1 (0x0202)
 *       - Pulse 3: Channel 2 (0x0404)
 *       - Pulse 4: Channel 3 (0x0808)
 *       - Pulse 5: Channel 4 (0x1010)
 *       - Pulse 6: Channel 5 (0x2020)
 *       - Pulse 7: Channel 6 (0x4040)
 *       - Pulse 8: Channel 7 (0x8080)
 */
#define MUX_PATTERN_PULSE_1  0x0101
#define MUX_PATTERN_PULSE_2  0x0202
#define MUX_PATTERN_PULSE_3  0x0404
#define MUX_PATTERN_PULSE_4  0x0808
#define MUX_PATTERN_PULSE_5  0x1010
#define MUX_PATTERN_PULSE_6  0x2020
#define MUX_PATTERN_PULSE_7  0x4040
#define MUX_PATTERN_PULSE_8  0x8080

/** @brief MUX pattern for PAUSE state (all channels off) */
#define MUX_PATTERN_PAUSE    0x0000

/** @} */ // end of mux_patterns

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

/*==============================================================================
 * VALIDATION MACROS (DO NOT MODIFY)
 *============================================================================*/

/* Compile-time checks to catch configuration errors */
#if TIMER_PULSE_IDX == TIMER_STATE_IDX
#error "TIMER_PULSE_IDX and TIMER_STATE_IDX must be different!"
#endif

#if ADC_INTERRUPT_BATCH_SIZE < 1 || ADC_INTERRUPT_BATCH_SIZE > 128
#error "ADC_INTERRUPT_BATCH_SIZE must be between 1 and 128"
#endif

#if MUX_ADVANCE_TIME_US < 10 || MUX_ADVANCE_TIME_US > 1000
#warning "MUX_ADVANCE_TIME_US is outside recommended range (10-1000µs)"
#endif

#endif /* CONFIG_H */
