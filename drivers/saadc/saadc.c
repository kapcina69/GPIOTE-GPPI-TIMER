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

static int16_t m_saadc_buffer[ADC_INTERRUPT_BATCH_SIZE];
static int16_t m_saadc_latest_sample = 0;
static volatile uint32_t sample_counter = 0;

static const nrfx_saadc_channel_t m_saadc_channel = {
    .channel_config = {
        .resistor_p = NRF_SAADC_RESISTOR_DISABLED,
        .resistor_n = NRF_SAADC_RESISTOR_DISABLED,
        .gain       = NRF_SAADC_GAIN1_6,
        .reference  = NRF_SAADC_REFERENCE_INTERNAL,
        .acq_time   = NRF_SAADC_ACQTIME_10US,
        .mode       = NRF_SAADC_MODE_SINGLE_ENDED,
        .burst      = NRF_SAADC_BURST_DISABLED,
    },
    .pin_p = SAADC_CHANNEL_AIN,
    .pin_n = NRF_SAADC_INPUT_DISABLED,
    .channel_index = 0,
};

int32_t saadc_sample_to_mv(int16_t sample)
{
    return ((int32_t)sample * 3600) / 1024;
}

static void saadc_handler(nrfx_saadc_evt_t const * p_event)
{
    switch (p_event->type)
    {
        case NRFX_SAADC_EVT_BUF_REQ:
            nrfx_saadc_buffer_set(m_saadc_buffer, ADC_INTERRUPT_BATCH_SIZE);
            break;

        case NRFX_SAADC_EVT_DONE:
            m_saadc_latest_sample = NRFX_SAADC_SAMPLE_GET(
                SAADC_RESOLUTION, 
                p_event->data.done.p_buffer, 
                ADC_INTERRUPT_BATCH_SIZE - 1
            );
            
            sample_counter += ADC_INTERRUPT_BATCH_SIZE;
            
#if ENABLE_ADC_LOGGING
            if (sample_counter % LOG_EVERY_N_SAMPLES == 0) {
                int32_t voltage_mv = saadc_sample_to_mv(m_saadc_latest_sample);
                printk("[ADC] #%u: %d mV\n", sample_counter, voltage_mv);
            }
#endif
            break;

        case NRFX_SAADC_EVT_FINISHED:
            nrfx_saadc_buffer_set(m_saadc_buffer, ADC_INTERRUPT_BATCH_SIZE);
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

    // Configure channel
    status = nrfx_saadc_channel_config(&m_saadc_channel);
    if (status != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("SAADC channel config failed: 0x%08X", status);
        return status;
    }

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

    // Set initial buffer
    status = nrfx_saadc_buffer_set(m_saadc_buffer, ADC_INTERRUPT_BATCH_SIZE);
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
    return m_saadc_latest_sample;
}

uint32_t saadc_get_sample_count(void)
{
    return sample_counter;
}
