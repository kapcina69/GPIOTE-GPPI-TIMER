/**
 * @file gpiote.c
 * @brief GPIOTE driver implementation
 * 
 * Configures GPIOTE for hardware-triggered output control.
 */

#include <nrfx_example.h>
#include "gpiote.h"
#include "../../config.h"
#include <nrfx_gpiote.h>

#undef NRFX_LOG_MODULE
#define NRFX_LOG_MODULE GPIOTE
#include <nrfx_log.h>

static nrfx_gpiote_t gpiote_inst = NRFX_GPIOTE_INSTANCE(GPIOTE_INST_IDX);

nrfx_err_t gpiote_init(uint8_t *ch_pin1, uint8_t *ch_pin2)
{
    nrfx_err_t status;
    
    // Connect GPIOTE IRQ handler
#if !NRFX_CHECK(NRFX_CONFIG_EXTERNAL_IRQ_HANDLING)
    IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_GPIOTE_INST_GET(GPIOTE_INST_IDX)), IRQ_PRIO_LOWEST,
                NRFX_GPIOTE_INST_HANDLER_GET(GPIOTE_INST_IDX), 0, 0);
#endif
    
    NRFX_LOG_INFO("Initializing GPIOTE...");
    
    // Initialize GPIOTE instance
    status = nrfx_gpiote_init(&gpiote_inst, NRFX_GPIOTE_DEFAULT_CONFIG_IRQ_PRIORITY);
    if (status != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("GPIOTE init failed: 0x%08X", status);
        return status;
    }
    
    // Allocate GPIOTE channels
    status = nrfx_gpiote_channel_alloc(&gpiote_inst, ch_pin1);
    if (status != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("Failed to allocate channel for PIN1: 0x%08X", status);
        return status;
    }
    
    status = nrfx_gpiote_channel_alloc(&gpiote_inst, ch_pin2);
    if (status != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("Failed to allocate channel for PIN2: 0x%08X", status);
        return status;
    }
    
    NRFX_LOG_INFO("GPIOTE channels allocated: PIN1=%d, PIN2=%d", *ch_pin1, *ch_pin2);
    
    // Configure output pins
    static const nrfx_gpiote_output_config_t output_config = {
        .drive = NRF_GPIO_PIN_S0S1,
        .input_connect = NRF_GPIO_PIN_INPUT_DISCONNECT,
        .pull = NRF_GPIO_PIN_NOPULL,
    };
    
    // Configure PIN1
    const nrfx_gpiote_task_config_t task_config_pin1 = {
        .task_ch = *ch_pin1,
        .polarity = NRF_GPIOTE_POLARITY_LOTOHI,
        .init_val = NRF_GPIOTE_INITIAL_VALUE_HIGH,
    };
    
    status = nrfx_gpiote_output_configure(&gpiote_inst, OUTPUT_PIN_1,
                                          &output_config, &task_config_pin1);
    if (status != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("Failed to configure PIN1: 0x%08X", status);
        return status;
    }
    nrfx_gpiote_out_task_enable(&gpiote_inst, OUTPUT_PIN_1);
    
    // Configure PIN2
    const nrfx_gpiote_task_config_t task_config_pin2 = {
        .task_ch = *ch_pin2,
        .polarity = NRF_GPIOTE_POLARITY_LOTOHI,
        .init_val = NRF_GPIOTE_INITIAL_VALUE_HIGH,
    };
    
    status = nrfx_gpiote_output_configure(&gpiote_inst, OUTPUT_PIN_2,
                                          &output_config, &task_config_pin2);
    if (status != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("Failed to configure PIN2: 0x%08X", status);
        return status;
    }
    nrfx_gpiote_out_task_enable(&gpiote_inst, OUTPUT_PIN_2);
    
    NRFX_LOG_INFO("GPIOTE configured: PIN1=%d (ch=%d), PIN2=%d (ch=%d)",
                 OUTPUT_PIN_1, *ch_pin1, OUTPUT_PIN_2, *ch_pin2);
    
    return NRFX_SUCCESS;
}
