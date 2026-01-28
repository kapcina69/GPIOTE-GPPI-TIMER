/**
 * @file dac.c
 * @brief SPI DAC Driver Implementation
 * 
 * OPTIMIZED FOR ISR CONTEXT - Non-blocking async SPI transfers
 */

#include "dac.h"
#include "../../config.h"
#include <hal/nrf_gpio.h>
#include <zephyr/kernel.h>

#define NRFX_LOG_MODULE DAC
#include <nrfx_log.h>

/* Internal state */
static uint8_t m_tx_buffer[3];  // Max 3 bytes for 16-bit DAC + command
static volatile bool m_transfer_pending = false;
static nrfx_spim_t *m_spim_ptr = NULL;

/**
 * @brief SPIM event handler for DAC
 */
static void dac_spim_handler(nrfx_spim_evt_t const *p_event, void *p_context)
{
    if (p_event->type == NRFX_SPIM_EVENT_DONE) {
        m_transfer_pending = false;
        
        /* De-assert CS */
        nrf_gpio_pin_set(DAC_CS_PIN);
        
        NRFX_LOG_DEBUG("DAC SPI transfer complete");
    }
}

nrfx_err_t dac_init(nrfx_spim_t *spim)
{
    printk("Starting DAC init...\n");
    if (spim == NULL) {
        return NRFX_ERROR_NULL;
    }

    // Connect SPIM IRQ handler
#if !NRFX_CHECK(NRFX_CONFIG_EXTERNAL_IRQ_HANDLING)
    IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_SPIM_INST_GET(DAC_SPIM_INST_IDX)), IRQ_PRIO_LOWEST,
                NRFX_SPIM_INST_HANDLER_GET(DAC_SPIM_INST_IDX), 0, 0);
#endif

    /* Configure CS pin */
    nrf_gpio_cfg_output(DAC_CS_PIN);
    nrf_gpio_pin_set(DAC_CS_PIN);  /* CS high = inactive */

    /* Configure SPIM */
    nrfx_spim_config_t config = NRFX_SPIM_DEFAULT_CONFIG(DAC_SCK_PIN, DAC_MOSI_PIN, 
                                                          18,
                                                          19);

    nrfx_err_t status = nrfx_spim_init(spim, &config, dac_spim_handler, NULL);
    if (status != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("DAC SPIM init failed: 0x%08X", status);
        return status;
                }
    /* Store SPIM pointer */
    m_spim_ptr = spim;

    /* Initial DAC value to 0 */
    nrf_gpio_pin_clear(DAC_CS_PIN);
    k_sleep(K_MSEC(1));
    nrf_gpio_pin_set(DAC_CS_PIN);

    NRFX_LOG_INFO("SPI DAC initialized (CS=P0.%d, MOSI=P0.%d, SCK=P0.%d)", 
                  DAC_CS_PIN, DAC_MOSI_PIN, DAC_SCK_PIN);
    printk("DAC initialized via SPI\n");

    return NRFX_SUCCESS;
}

nrfx_err_t dac_set_value(uint16_t value)
{
    /* Check if previous transfer is still ongoing */
    if (m_transfer_pending) {
        NRFX_LOG_DEBUG("DAC busy, dropping value");
        return NRFX_ERROR_BUSY;
    }

    /* Clamp to valid range */
    if (value > DAC_MAX_VALUE) {
        value = DAC_MAX_VALUE;
    }

    /* 
     * Prepare SPI data (format depends on your DAC chip)
     * Common formats:
     * - MCP4921: [cmd(4bit) | value(12bit)] = 2 bytes
     * - AD5328: [addr(4bit) | data(12bit)] = 2 bytes
     * - DAC8562: [cmd(8bit) | data(16bit)] = 3 bytes
     * 
     * Example for 12-bit DAC with command byte:
     */
    m_tx_buffer[0] = 0x30;  // Command byte (adjust for your DAC)
    m_tx_buffer[1] = (uint8_t)(value >> 4);   // Upper 8 bits
    m_tx_buffer[2] = (uint8_t)(value << 4);   // Lower 4 bits

    /* Assert CS before transfer */
    nrf_gpio_pin_clear(DAC_CS_PIN);

    /* TX-only transfer */
    nrfx_spim_xfer_desc_t xfer = NRFX_SPIM_XFER_TX(m_tx_buffer, 3);

    /* Start non-blocking transfer */
    m_transfer_pending = true;
    
    nrfx_err_t err = nrfx_spim_xfer(m_spim_ptr, &xfer, 0);
    
    if (err != NRFX_SUCCESS) {
        m_transfer_pending = false;
        nrf_gpio_pin_set(DAC_CS_PIN);  /* Release CS on error */
        NRFX_LOG_WARNING("DAC SPI transfer failed: 0x%08X", err);
        return err;
    }

    /* Return immediately - handler will de-assert CS when done */
    return NRFX_SUCCESS;
}

bool dac_is_ready(void)
{
    return !m_transfer_pending;
}

void dac_wait_ready(void)
{
    while (m_transfer_pending) {
        /* Busy wait */
    }
}