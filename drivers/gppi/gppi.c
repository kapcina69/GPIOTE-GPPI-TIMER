/**
 * @file gppi.c
 * @brief GPPI driver implementation
 * 
 * Manages GPPI connections for hardware-triggered pulse generation and ADC sampling.
 * 
 * NEW MODE: Only LED1 (PIN1) toggles via GPPI. LED2 (PIN2) stays LOW.
 */

#include "gppi.h"
#include "../../config.h"
#include "../timers/timer.h"
#include "../mux/mux.h"
#include "../dac/dac.h"
#include <helpers/nrfx_gppi.h>
#include <nrfx_timer.h>
#include <hal/nrf_saadc.h>
#include <hal/nrf_gpiote.h>

#define NRFX_LOG_MODULE GPPI
#include <nrfx_log.h>

// GPPI channel allocations
static uint8_t gppi_pin1_set;
static uint8_t gppi_pin1_clr;
static uint8_t gppi_adc_trigger;
static uint8_t gppi_adc_capture;
static uint8_t gppi_mux_trigger;
static uint8_t gppi_dac_trigger;

nrfx_err_t gppi_init(void)
{
    nrfx_err_t status;
    
    NRFX_LOG_INFO("Initializing GPPI channels (LED1 only mode)...");
    
    // Allocate GPPI channels for PIN1 control
    status = nrfx_gppi_channel_alloc(&gppi_pin1_set);
    if (status != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("Failed to allocate gppi_pin1_set: 0x%08X", status);
        return status;
    }
    
    status = nrfx_gppi_channel_alloc(&gppi_pin1_clr);
    if (status != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("Failed to allocate gppi_pin1_clr: 0x%08X", status);
        return status;
    }
    
    // NOTE: PIN2 GPPI channels removed - LED2 stays LOW in new mode
    
    // Allocate GPPI channels for ADC
    status = nrfx_gppi_channel_alloc(&gppi_adc_trigger);
    if (status != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("Failed to allocate gppi_adc_trigger: 0x%08X", status);
        return status;
    }
    
    status = nrfx_gppi_channel_alloc(&gppi_adc_capture);
    if (status != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("Failed to allocate gppi_adc_capture: 0x%08X", status);
        return status;
    }

    status = nrfx_gppi_channel_alloc(&gppi_mux_trigger);
    if (status != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("Failed to allocate gppi_mux_trigger: 0x%08X", status);
        return status;
    }

    status = nrfx_gppi_channel_alloc(&gppi_dac_trigger);
    if (status != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("Failed to allocate gppi_dac_trigger: 0x%08X", status);
        return status;
    }
    
    NRFX_LOG_INFO("GPPI channels allocated: PIN1=%d/%d, ADC=%d/%d, MUX=%d, DAC=%d (PIN2 disabled)",
                 gppi_pin1_set, gppi_pin1_clr,
                 gppi_adc_trigger, gppi_adc_capture, gppi_mux_trigger, gppi_dac_trigger);
    
    return NRFX_SUCCESS;
}

nrfx_err_t gppi_setup_connections(uint8_t gpiote_ch_pin1, uint8_t gpiote_ch_pin2)
{
    (void)gpiote_ch_pin2;  // PIN2 not used in LED1-only mode
    
    // Get timer instance to access compare event addresses
    nrfx_timer_t *timer_pulse_ptr = NULL;
    nrfx_timer_t *timer_state_ptr = NULL;
    timer_get_instances(&timer_pulse_ptr, &timer_state_ptr);
    
    if ((timer_pulse_ptr == NULL) || (timer_state_ptr == NULL)) {
        NRFX_LOG_ERROR("Timer instance not available");
        return NRFX_ERROR_NULL;
    }
    
    NRFX_LOG_INFO("Setting up GPPI connections (LED1 only)...");
    
    // Get GPIOTE task addresses for PIN1 only
    uint32_t pin1_set_addr = (uint32_t)&NRF_GPIOTE->TASKS_SET[gpiote_ch_pin1];
    uint32_t pin1_clr_addr = (uint32_t)&NRF_GPIOTE->TASKS_CLR[gpiote_ch_pin1];
    
    // Get Timer compare event addresses
    uint32_t timer_cc0_event = nrfx_timer_compare_event_address_get(timer_pulse_ptr, NRF_TIMER_CC_CHANNEL0);
    uint32_t timer_cc1_event = nrfx_timer_compare_event_address_get(timer_pulse_ptr, NRF_TIMER_CC_CHANNEL1);
    uint32_t timer_state_cc1_event = nrfx_timer_compare_event_address_get(timer_state_ptr, NRF_TIMER_CC_CHANNEL1);
    
    // Get SAADC task/event addresses
    uint32_t saadc_sample_task = nrf_saadc_task_address_get(NRF_SAADC, NRF_SAADC_TASK_SAMPLE);
    uint32_t saadc_end_event = nrf_saadc_event_address_get(NRF_SAADC, NRF_SAADC_EVENT_END);
    uint32_t timer_capture_task = nrfx_timer_task_address_get(timer_pulse_ptr, NRF_TIMER_TASK_CAPTURE4);
    uint32_t spim_start_task = mux_start_task_address_get();
    uint32_t dac_start_task = dac_start_task_address_get();
    
    // Setup GPPI connections for PIN1 only
    // PIN1: CC0 triggers clear (pulse start/LOW = active), CC1 triggers set (pulse end/HIGH = inactive)
    nrfx_gppi_channel_endpoints_setup(gppi_pin1_set, timer_cc0_event, pin1_clr_addr);
    nrfx_gppi_channel_endpoints_setup(gppi_pin1_clr, timer_cc1_event, pin1_set_addr);
    
    // NOTE: PIN2 GPPI connections removed - LED2 stays LOW
    
    // ADC: CC0 triggers sample (when pin goes LOW = active), END event captures timestamp
    nrfx_gppi_channel_endpoints_setup(gppi_adc_trigger, timer_cc0_event, saadc_sample_task);
    nrfx_gppi_channel_endpoints_setup(gppi_adc_capture, saadc_end_event, timer_capture_task);

    // MUX: state timer CC1 event triggers SPIM START task for preloaded transfer
    nrfx_gppi_channel_endpoints_setup(gppi_mux_trigger, timer_state_cc1_event, spim_start_task);
    // DAC: same state timer CC1 event triggers DAC SPIM START at the same instant
    nrfx_gppi_channel_endpoints_setup(gppi_dac_trigger, timer_state_cc1_event, dac_start_task);
    
    NRFX_LOG_INFO("GPPI connections configured (LED1 only, LED2 stays LOW)");
    
    return NRFX_SUCCESS;
}

void gppi_enable(void)
{
    // Only enable PIN1 and ADC channels - PIN2 channels removed
    uint32_t channels_mask = BIT(gppi_pin1_set) | BIT(gppi_pin1_clr) |
                             BIT(gppi_adc_trigger) | BIT(gppi_adc_capture) |
                             BIT(gppi_mux_trigger) | BIT(gppi_dac_trigger);
    
    nrfx_gppi_channels_enable(channels_mask);
    
    NRFX_LOG_INFO("GPPI channels enabled (mask: 0x%08X) - LED1 only mode", channels_mask);
}
