/**
 * @file saadc.h
 * @brief SAADC driver interface
 * 
 * Configures SAADC for hardware-triggered sampling.
 * Provides API for initialization and accessing latest sample data.
 */

#ifndef SAADC_H
#define SAADC_H

#include <nrfx_saadc.h>
#include <stdint.h>

/**
 * @brief Initialize SAADC for hardware-triggered sampling
 * 
 * Configures SAADC with single-ended mode, internal reference,
 * and sets up continuous sampling with double buffering.
 * 
 * @return NRFX_SUCCESS on success, error code otherwise
 */
nrfx_err_t saadc_init(void);

/**
 * @brief Get the latest SAADC sample value from channel 0
 * 
 * @return Latest ADC sample from CH0 (raw value)
 */
int16_t saadc_get_latest_sample(void);

#if SAADC_DUAL_CHANNEL_ENABLED
/**
 * @brief Get the latest SAADC sample value from channel 1
 * 
 * @return Latest ADC sample from CH1 (raw value)
 */
int16_t saadc_get_latest_sample_ch1(void);
#endif

/**
 * @brief Get total number of samples captured
 * 
 * @return Total sample count
 */
uint32_t saadc_get_sample_count(void);

/**
 * @brief Convert SAADC sample to millivolts
 * 
 * @param sample Raw SAADC sample value
 * @return Voltage in millivolts
 */
int32_t saadc_sample_to_mv(int16_t sample);

#endif // SAADC_H
