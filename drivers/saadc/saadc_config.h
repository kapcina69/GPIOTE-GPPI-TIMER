/**
 * @file saadc_config.h
 * @brief SAADC Driver Configuration
 * 
 * Configuration parameters specific to the SAADC (ADC) driver.
 */

#ifndef SAADC_CONFIG_H
#define SAADC_CONFIG_H

#include <hal/nrf_saadc.h>

/*==============================================================================
 * ADC CHANNEL CONFIGURATION
 *============================================================================*/

/** 
 * @brief SAADC input channel 0 selection
 * @note Use NRF_SAADC_INPUT_AIN0 through NRF_SAADC_INPUT_AIN7 for available inputs
 */
#define SAADC_CHANNEL0_AIN NRF_SAADC_INPUT_AIN0

/**
 * @brief Enable second ADC channel
 * @note Set to 1 to enable dual-channel sampling, 0 for single channel only
 */
#define SAADC_DUAL_CHANNEL_ENABLED 0

#if SAADC_DUAL_CHANNEL_ENABLED
/** 
 * @brief SAADC input channel 1 selection
 * @note Second ADC channel samples in parallel with channel 0
 */
#define SAADC_CHANNEL1_AIN NRF_SAADC_INPUT_AIN3

/**
 * @brief Number of ADC channels
 */
#define SAADC_CHANNEL_COUNT 2
#else
/**
 * @brief Number of ADC channels
 */
#define SAADC_CHANNEL_COUNT 1
#endif

/*==============================================================================
 * ADC SAMPLING CONFIGURATION
 *============================================================================*/

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

/** 
 * @brief Enable ADC sample logging
 * @note When enabled, logs ADC samples to console (respecting LOG_EVERY_N_SAMPLES).
 *       Useful for verifying ADC operation but increases power consumption.
 *       
 *       Set to 0 to disable all ADC logging (lowest power).
 *       Set to 1 to enable periodic ADC logging.
 */
#define ENABLE_ADC_LOGGING 1

/*==============================================================================
 * VALIDATION
 *============================================================================*/

#if ADC_INTERRUPT_BATCH_SIZE < 1 || ADC_INTERRUPT_BATCH_SIZE > 128
#error "ADC_INTERRUPT_BATCH_SIZE must be between 1 and 128"
#endif

#endif /* SAADC_CONFIG_H */
