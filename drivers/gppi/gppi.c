/**
 * @file gppi.c
 * @brief GPPI driver implementation
 * 
 * Manages GPPI connections for hardware-triggered pulse generation and ADC sampling.
 */

#include "gppi.h"
#include "../../config.h"
#include "../timers/timer.h"
#include <helpers/nrfx_gppi.h>
#include <nrfx_timer.h>
#include <hal/nrf_saadc.h>
#include <hal/nrf_gpiote.h>

#define NRFX_LOG_MODULE GPPI
#include <nrfx_log.h>

// GPPI channel allocations
static uint8_t gppi_pin1_set;
static uint8_t gppi_pin1_clr;
static uint8_t gppi_pin2_set;
static uint8_t gppi_pin2_clr;
static uint8_t gppi_adc_trigger;
static uint8_t gppi_adc_capture;

nrfx_err_t gppi_init(void)
{
    nrfx_err_t status;
    
    NRFX_LOG_INFO("Initializing GPPI channels...");
    
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
    
    // Allocate GPPI channels for PIN2 control
    status = nrfx_gppi_channel_alloc(&gppi_pin2_set);
    if (status != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("Failed to allocate gppi_pin2_set: 0x%08X", status);
        return status;
    }
    
    status = nrfx_gppi_channel_alloc(&gppi_pin2_clr);
    if (status != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("Failed to allocate gppi_pin2_clr: 0x%08X", status);
        return status;
    }
    
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
    
    NRFX_LOG_INFO("GPPI channels allocated: PIN1=%d/%d, PIN2=%d/%d, ADC=%d/%d",
                 gppi_pin1_set, gppi_pin1_clr,
                 gppi_pin2_set, gppi_pin2_clr,
                 gppi_adc_trigger, gppi_adc_capture);
    
    return NRFX_SUCCESS;
}

nrfx_err_t gppi_setup_connections(uint8_t gpiote_ch_pin1, uint8_t gpiote_ch_pin2)
{
    // Get timer instance to access compare event addresses
    nrfx_timer_t *timer_pulse_ptr = NULL;
    timer_get_instances(&timer_pulse_ptr, NULL);
    
    if (timer_pulse_ptr == NULL) {
        NRFX_LOG_ERROR("Timer instance not available");
        return NRFX_ERROR_NULL;
    }
    
    NRFX_LOG_INFO("Setting up GPPI connections...");
    
    // Get GPIOTE task addresses
    uint32_t pin1_set_addr = (uint32_t)&NRF_GPIOTE->TASKS_SET[gpiote_ch_pin1];
    uint32_t pin1_clr_addr = (uint32_t)&NRF_GPIOTE->TASKS_CLR[gpiote_ch_pin1];
    uint32_t pin2_set_addr = (uint32_t)&NRF_GPIOTE->TASKS_SET[gpiote_ch_pin2];
    uint32_t pin2_clr_addr = (uint32_t)&NRF_GPIOTE->TASKS_CLR[gpiote_ch_pin2];
    
    // Get Timer compare event addresses
    uint32_t timer_cc0_event = nrfx_timer_compare_event_address_get(timer_pulse_ptr, NRF_TIMER_CC_CHANNEL0);
    uint32_t timer_cc1_event = nrfx_timer_compare_event_address_get(timer_pulse_ptr, NRF_TIMER_CC_CHANNEL1);
    uint32_t timer_cc2_event = nrfx_timer_compare_event_address_get(timer_pulse_ptr, NRF_TIMER_CC_CHANNEL2);
    uint32_t timer_cc3_event = nrfx_timer_compare_event_address_get(timer_pulse_ptr, NRF_TIMER_CC_CHANNEL3);
    
    // Get SAADC task/event addresses
    uint32_t saadc_sample_task = nrf_saadc_task_address_get(NRF_SAADC, NRF_SAADC_TASK_SAMPLE);
    uint32_t saadc_end_event = nrf_saadc_event_address_get(NRF_SAADC, NRF_SAADC_EVENT_END);
    uint32_t timer_capture_task = nrfx_timer_task_address_get(timer_pulse_ptr, NRF_TIMER_TASK_CAPTURE4);
    
    // Setup GPPI connections
    // PIN1: CC0 triggers clear (pulse start), CC1 triggers set (pulse end)
    nrfx_gppi_channel_endpoints_setup(gppi_pin1_set, timer_cc0_event, pin1_clr_addr);
    nrfx_gppi_channel_endpoints_setup(gppi_pin1_clr, timer_cc1_event, pin1_set_addr);
    
    // PIN2: CC2 triggers clear (pulse start), CC3 triggers set (pulse end)
    nrfx_gppi_channel_endpoints_setup(gppi_pin2_set, timer_cc2_event, pin2_clr_addr);
    nrfx_gppi_channel_endpoints_setup(gppi_pin2_clr, timer_cc3_event, pin2_set_addr);
    
    // ADC: CC1 triggers sample, END event captures timestamp
    nrfx_gppi_channel_endpoints_setup(gppi_adc_trigger, timer_cc1_event, saadc_sample_task);
    nrfx_gppi_channel_endpoints_setup(gppi_adc_capture, saadc_end_event, timer_capture_task);
    
    NRFX_LOG_INFO("GPPI connections configured");
    
    return NRFX_SUCCESS;
}

void gppi_enable(void)
{
    uint32_t channels_mask = BIT(gppi_pin1_set) | BIT(gppi_pin1_clr) |
                             BIT(gppi_pin2_set) | BIT(gppi_pin2_clr) |
                             BIT(gppi_adc_trigger) | BIT(gppi_adc_capture);
    
    nrfx_gppi_channels_enable(channels_mask);
    
    NRFX_LOG_INFO("GPPI channels enabled (mask: 0x%08X)", channels_mask);
}
