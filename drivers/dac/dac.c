/**
 * @file dac.c
 * @brief MCP4725 DAC Driver Implementation
 * 
 * Uses nrfx_twim for I2C communication (no device tree overlay needed)
 */

#include "dac.h"
#include "../../config.h"

#define NRFX_LOG_MODULE DAC
#include <nrfx_twim.h>
#include <nrfx_log.h>
#include <zephyr/kernel.h>

#if defined(NRF52_SERIES)
#define DAC_TWIM_INST_IDX 0
#else
#define DAC_TWIM_INST_IDX 1
#endif

// Export twim_inst for test functions (non-static)
nrfx_twim_t twim_inst = NRFX_TWIM_INSTANCE(DAC_TWIM_INST_IDX);
static bool dac_initialized = false;

// Transfer buffer - must be static for async operation
static uint8_t tx_buffer[3];
static volatile bool transfer_in_progress = false;

/**
 * @brief TWIM event handler for DAC operations
 */
static void dac_twim_handler(nrfx_twim_evt_t const * p_event, void * p_context)
{
    switch (p_event->type)
    {
        case NRFX_TWIM_EVT_DONE:
            transfer_in_progress = false;
            NRFX_LOG_DEBUG("DAC TWIM transfer done");
            break;
            
        case NRFX_TWIM_EVT_ADDRESS_NACK:
            transfer_in_progress = false;
            NRFX_LOG_WARNING("DAC TWIM address NACK");
            break;
            
        case NRFX_TWIM_EVT_DATA_NACK:
            transfer_in_progress = false;
            NRFX_LOG_WARNING("DAC TWIM data NACK");
            break;
            
        default:
            transfer_in_progress = false;
            break;
    }
}

nrfx_err_t dac_init(void)
{
    nrfx_err_t status;

    // Configure I2C pins and settings  
    nrfx_twim_config_t twim_config = NRFX_TWIM_DEFAULT_CONFIG(DAC_SCL_PIN, DAC_SDA_PIN);
    twim_config.frequency = NRF_TWIM_FREQ_100K;  // 100 kHz standard I2C

    // Initialize TWIM (I2C) peripheral
    status = nrfx_twim_init(&twim_inst, &twim_config, dac_twim_handler, NULL);
    if (status != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("TWIM init failed: 0x%08X", status);
        return status;
    }

    nrfx_twim_enable(&twim_inst);
    dac_initialized = true;

    NRFX_LOG_INFO("DAC (MCP4725) initialized on I2C addr 0x%02X", DAC_I2C_ADDR);
    printk("DAC initialized (SDA=P0.%d, SCL=P0.%d)\n", DAC_SDA_PIN, DAC_SCL_PIN);

    return NRFX_SUCCESS;
}

void dac_set_value(uint16_t value)
{
    if (!dac_initialized) {
        NRFX_LOG_WARNING("DAC not initialized");
        return;
    }

    // Check if previous transfer is still in progress
    if (transfer_in_progress) {
        NRFX_LOG_DEBUG("DAC busy, dropping value %u", value);
        return;  // Drop this command to avoid race condition
    }

    // Clamp to 10-bit range (MCP4725 is 12-bit but we use 10-bit)
    if (value > 0x03FF) {
        value = 0x03FF;
    }

    // MCP4725 fast write mode format (using static buffer for async safety)
    // Byte 0: Command (0x40 = write DAC register)
    // Byte 1: D11-D4 (upper 8 bits)
    // Byte 2: D3-D0 (lower 4 bits) in upper nibble
    tx_buffer[0] = 0x40;                    // Fast mode write command
    tx_buffer[1] = (value >> 2) & 0xFF;     // Upper 8 bits
    tx_buffer[2] = (value & 0x03) << 6;     // Lower 2 bits shifted

    transfer_in_progress = true;
    
    nrfx_twim_xfer_desc_t xfer = NRFX_TWIM_XFER_DESC_TX(DAC_I2C_ADDR, tx_buffer, sizeof(tx_buffer));
    nrfx_err_t status = nrfx_twim_xfer(&twim_inst, &xfer, 0);

    if (status != NRFX_SUCCESS) {
        transfer_in_progress = false;
        NRFX_LOG_WARNING("DAC write error: val=%u, err=0x%08X", value, status);
    }
}

bool dac_is_ready(void)
{
    return dac_initialized;
}
