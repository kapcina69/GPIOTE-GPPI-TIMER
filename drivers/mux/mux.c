/**
 * @file mux.c
 * @brief Multiplexer driver implementation for SPI-based channel control
 * OPTIMIZED FOR ISR CONTEXT - Non-blocking async transfers
 *
 * Copyright (c) 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <nrfx_example.h>
#include "mux.h"
#include "../../config.h"
#include <hal/nrf_gpio.h>
#include <zephyr/kernel.h>

#undef NRFX_LOG_MODULE
#define NRFX_LOG_MODULE MUX
#include <nrfx_log.h>

/* SPI pins - using loopback pins from nrfx_example.h */
#define MOSI_PIN LOOPBACK_PIN_1A
#define MISO_PIN LOOPBACK_PIN_1B
#define SCK_PIN  LOOPBACK_PIN_2A

/* Internal state */
static uint8_t m_tx_buffer[2];
static uint8_t m_rx_buffer[2];
static volatile bool m_xfer_done = false;
static volatile bool m_transfer_pending = false;
static nrfx_spim_t *m_spim_ptr = NULL;

/**
 * @brief SPIM event handler - called when SPI transfer completes
 */
static void spim_handler(nrfx_spim_evt_t const *p_event, void *p_context)
{
    if (p_event->type == NRFX_SPIM_EVENT_DONE) {
        m_xfer_done = true;
        m_transfer_pending = false;
        
        /* Pulse LE (Latch Enable) to latch data into MUX registers */
        nrf_gpio_pin_clear(MUX_LE_PIN);
        __NOP(); __NOP(); __NOP(); __NOP();  /* ~50-100ns @ 64MHz */
        nrf_gpio_pin_set(MUX_LE_PIN);
    }
}

nrfx_err_t mux_init(nrfx_spim_t *spim)
{
    if (spim == NULL) {
        return NRFX_ERROR_NULL;
    }

    // Connect SPIM IRQ handler
#if !NRFX_CHECK(NRFX_CONFIG_EXTERNAL_IRQ_HANDLING)
    IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_SPIM_INST_GET(SPIM_INST_IDX)), IRQ_PRIO_LOWEST,
                NRFX_SPIM_INST_HANDLER_GET(SPIM_INST_IDX), 0, 0);
#endif

    /* Configure GPIO pins for MUX control */
    nrf_gpio_cfg_output(MUX_LE_PIN);
    nrf_gpio_pin_set(MUX_LE_PIN);  /* LE high = disabled/latched */
    
    nrf_gpio_cfg_output(MUX_CLR_PIN);
    nrf_gpio_pin_clear(MUX_CLR_PIN);  /* CLR low = not clearing */

    /* Configure and initialize SPIM */
    nrfx_spim_config_t config = NRFX_SPIM_DEFAULT_CONFIG(SCK_PIN, MOSI_PIN, MISO_PIN, 
                                                          NRF_SPIM_PIN_NOT_CONNECTED);
    
    nrfx_err_t status = nrfx_spim_init(spim, &config, spim_handler, NULL);
    if (status != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("SPIM init failed: 0x%08X", status);
        return status;
    }

    /* Store SPIM pointer for mux_write */
    m_spim_ptr = spim;

    /* Clear all MUX channels */
    nrf_gpio_pin_set(MUX_CLR_PIN);
    k_sleep(K_MSEC(10));
    nrf_gpio_pin_clear(MUX_CLR_PIN);

    NRFX_LOG_INFO("MUX initialized: %d channels", MUX_NUM_CHANNELS);

    return NRFX_SUCCESS;
}

/**
 * @brief Non-blocking MUX write - safe to call from ISR
 * 
 * @param data 16-bit pattern to write to MUX
 * @return NRFX_SUCCESS if transfer started
 *         NRFX_ERROR_BUSY if previous transfer still in progress
 */
nrfx_err_t mux_write(uint16_t data)
{
    /* Check if previous transfer is still ongoing */
    if (m_transfer_pending) {
        return NRFX_ERROR_BUSY;  /* Transfer already in progress */
    }

    /* Prepare data */
    m_tx_buffer[0] = (uint8_t)(data >> 8);
    m_tx_buffer[1] = (uint8_t)data;

    /* TX-only transfer (no RX needed for MUX) */
    nrfx_spim_xfer_desc_t xfer = NRFX_SPIM_XFER_TX(m_tx_buffer, 2);

    /* Start NON-BLOCKING transfer */
    m_xfer_done = false;
    m_transfer_pending = true;
    
    nrfx_err_t err = nrfx_spim_xfer(m_spim_ptr, &xfer, 0);
    
    if (err != NRFX_SUCCESS) {
        m_transfer_pending = false;
        return err;
    }

    /* Return immediately - spim_handler will latch data when done */
    return NRFX_SUCCESS;
}

/**
 * @brief Check if MUX transfer is complete
 * 
 * @return true if no transfer in progress, false otherwise
 */
bool mux_is_ready(void)
{
    return !m_transfer_pending;
}

/**
 * @brief Wait for MUX transfer to complete (blocking)
 * Only use this outside of ISR context!
 */
void mux_wait_ready(void)
{
    while (m_transfer_pending) {
        /* Busy wait */
    }
}
