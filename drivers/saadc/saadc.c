/**
 * @file saadc.c
 * @brief SAADC driver implementation
 * 
 * Configures SAADC for hardware-triggered continuous sampling.
 */

#include <nrfx_example.h>
#include "saadc.h"
#include "../../config.h"
#include <nrfx_saadc.h>
#include <hal/nrf_saadc.h>

#undef NRFX_LOG_MODULE
#define NRFX_LOG_MODULE SAADC
#include <nrfx_log.h>

// Buffer size = batch size * channel count (interleaved: [CH0, CH1, CH0, CH1, ...])
static int16_t m_saadc_buffer[ADC_INTERRUPT_BATCH_SIZE * SAADC_CHANNEL_COUNT];
static int16_t m_saadc_latest_sample_ch0 = 0;
#if SAADC_DUAL_CHANNEL_ENABLED
static int16_t m_saadc_latest_sample_ch1 = 0;
#endif
static volatile uint32_t sample_counter = 0;

static const nrfx_saadc_channel_t m_saadc_channel0 = {
    .channel_config = {
        .resistor_p = NRF_SAADC_RESISTOR_DISABLED,
        .resistor_n = NRF_SAADC_RESISTOR_DISABLED,
        .gain       = NRF_SAADC_GAIN1_6,
        .reference  = NRF_SAADC_REFERENCE_INTERNAL,
        .acq_time   = NRF_SAADC_ACQTIME_10US,
        .mode       = NRF_SAADC_MODE_SINGLE_ENDED,
        .burst      = NRF_SAADC_BURST_DISABLED,
    },
    .pin_p = SAADC_CHANNEL0_AIN,
    .pin_n = NRF_SAADC_INPUT_DISABLED,
    .channel_index = 0,
};

#if SAADC_DUAL_CHANNEL_ENABLED
static const nrfx_saadc_channel_t m_saadc_channel1 = {
    .channel_config = {
        .resistor_p = NRF_SAADC_RESISTOR_DISABLED,
        .resistor_n = NRF_SAADC_RESISTOR_DISABLED,
        .gain       = NRF_SAADC_GAIN1_6,
        .reference  = NRF_SAADC_REFERENCE_INTERNAL,
        .acq_time   = NRF_SAADC_ACQTIME_10US,
        .mode       = NRF_SAADC_MODE_SINGLE_ENDED,
        .burst      = NRF_SAADC_BURST_DISABLED,
    },
    .pin_p = SAADC_CHANNEL1_AIN,
    .pin_n = NRF_SAADC_INPUT_DISABLED,
    .channel_index = 1,
};
#endif

int32_t saadc_sample_to_mv(int16_t sample)
{
    return ((int32_t)sample * 3600) / 1024;
}

static void saadc_handler(nrfx_saadc_evt_t const * p_event)
{
    switch (p_event->type)
    {
        case NRFX_SAADC_EVT_BUF_REQ:
            nrfx_saadc_buffer_set(m_saadc_buffer, ADC_INTERRUPT_BATCH_SIZE * SAADC_CHANNEL_COUNT);
            break;

        case NRFX_SAADC_EVT_DONE:
#if SAADC_DUAL_CHANNEL_ENABLED
            // Buffer is interleaved: [CH0, CH1, CH0, CH1, ...]
            // Get last sample from each channel
            uint16_t total_samples = ADC_INTERRUPT_BATCH_SIZE * SAADC_CHANNEL_COUNT;
            m_saadc_latest_sample_ch0 = NRFX_SAADC_SAMPLE_GET(
                SAADC_RESOLUTION, 
                p_event->data.done.p_buffer, 
                total_samples - 2  // Second-to-last = CH0
            );
            m_saadc_latest_sample_ch1 = NRFX_SAADC_SAMPLE_GET(
                SAADC_RESOLUTION, 
                p_event->data.done.p_buffer, 
                total_samples - 1  // Last = CH1
            );
#else
            // Single channel mode
            m_saadc_latest_sample_ch0 = NRFX_SAADC_SAMPLE_GET(
                SAADC_RESOLUTION, 
                p_event->data.done.p_buffer, 
                ADC_INTERRUPT_BATCH_SIZE - 1
            );
#endif
            
            sample_counter += ADC_INTERRUPT_BATCH_SIZE;
            
#if ENABLE_ADC_LOGGING
            if (sample_counter % LOG_EVERY_N_SAMPLES == 0) {
                int32_t voltage_ch0 = saadc_sample_to_mv(m_saadc_latest_sample_ch0);
#if SAADC_DUAL_CHANNEL_ENABLED
                int32_t voltage_ch1 = saadc_sample_to_mv(m_saadc_latest_sample_ch1);
                NRFX_LOG_INFO("[ADC] #%u: CH0=%d mV, CH1=%d mV", sample_counter, voltage_ch0, voltage_ch1);
#else
                NRFX_LOG_INFO("[ADC] #%u: %d mV", sample_counter, voltage_ch0);
#endif
            }
#endif
            break;

        case NRFX_SAADC_EVT_FINISHED:
            nrfx_saadc_buffer_set(m_saadc_buffer, ADC_INTERRUPT_BATCH_SIZE * SAADC_CHANNEL_COUNT);
            nrfx_saadc_mode_trigger();
            break;

        default:
            break;
    }
}

nrfx_err_t saadc_init(void)
{
    nrfx_err_t status;
    
    // Connect SAADC IRQ handler
#if !NRFX_CHECK(NRFX_CONFIG_EXTERNAL_IRQ_HANDLING)
    IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_SAADC), IRQ_PRIO_LOWEST, 
                nrfx_saadc_irq_handler, 0, 0);
#endif
    
    NRFX_LOG_INFO("Initializing SAADC...");
    
    // Initialize SAADC
    status = nrfx_saadc_init(NRFX_SAADC_DEFAULT_CONFIG_IRQ_PRIORITY);
    if (status != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("SAADC init failed: 0x%08X", status);
        return status;
    }

    // Configure channel 0
    status = nrfx_saadc_channel_config(&m_saadc_channel0);
    if (status != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("SAADC channel 0 config failed: 0x%08X", status);
        return status;
    }

#if SAADC_DUAL_CHANNEL_ENABLED
    // Configure channel 1
    status = nrfx_saadc_channel_config(&m_saadc_channel1);
    if (status != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("SAADC channel 1 config failed: 0x%08X", status);
        return status;
    }
#endif

    // Configure advanced mode
    uint32_t channels_mask = nrfx_saadc_channels_configured_get();
    status = nrfx_saadc_advanced_mode_set(channels_mask,
                                           SAADC_RESOLUTION,
                                           &(nrfx_saadc_adv_config_t){
                                               .oversampling = NRF_SAADC_OVERSAMPLE_DISABLED,
                                               .burst = NRF_SAADC_BURST_DISABLED,
                                               .internal_timer_cc = 0,
                                               .start_on_end = true,
                                           },
                                           saadc_handler);
    if (status != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("SAADC advanced mode failed: 0x%08X", status);
        return status;
    }

    // Set initial buffer (size = batch * channels)
    status = nrfx_saadc_buffer_set(m_saadc_buffer, ADC_INTERRUPT_BATCH_SIZE * SAADC_CHANNEL_COUNT);
    if (status != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("SAADC buffer set failed: 0x%08X", status);
        return status;
    }
    
    // Enable and trigger
    nrf_saadc_enable(NRF_SAADC);
    status = nrfx_saadc_mode_trigger();
    if (status != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("SAADC mode trigger failed: 0x%08X", status);
        return status;
    }
    
    NRFX_LOG_INFO("SAADC initialized successfully");
    return NRFX_SUCCESS;
}

int16_t saadc_get_latest_sample(void)
{
    return m_saadc_latest_sample_ch0;
}

#if SAADC_DUAL_CHANNEL_ENABLED
int16_t saadc_get_latest_sample_ch1(void)
{
    return m_saadc_latest_sample_ch1;
}
#endif

uint32_t saadc_get_sample_count(void)
{
    return sample_counter;
}
