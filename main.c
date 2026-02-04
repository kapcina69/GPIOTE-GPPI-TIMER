/*
 * ULTRA LOW POWER VERSION with DUAL TIMER CHANNEL MUX PRE-LOADING
 * 
 * State timer uses TWO compare channels:
 * - CC_CHANNEL0: State transition event (main timing)
 * - CC_CHANNEL1: MUX pre-load event (200-500us before state transition)
 * 
 * This ensures MUX pattern arrives BEFORE the pulse starts!
 */

#include <nrfx_example.h>
#include <helpers/nrfx_gppi.h>
#include <nrfx_timer.h>
#include "config.h"
#include "services/ble.h"
#include "drivers/mux/mux.h"
#include "drivers/timers/timer.h"
#include "drivers/gppi/gppi.h"
#include "drivers/gpiote/gpiote.h"
#include "drivers/saadc/saadc.h"
#include "drivers/dac/dac.h"
#include "drivers/UART/uart.h"

#include <nrfx_log.h>

// GPIOTE channels
static uint8_t gpiote_ch_pin1;
static uint8_t gpiote_ch_pin2;

#if ENABLE_STATS_TIMER
static void stats_timer_callback(struct k_timer *timer);
K_TIMER_DEFINE(stats_timer, stats_timer_callback, NULL);
static uint32_t last_sample_count = 0;
#endif

#if ENABLE_STATS_TIMER
static void stats_timer_callback(struct k_timer *timer)
{
    uint32_t current_count = saadc_get_sample_count();
    uint32_t samples_since_last = current_count - last_sample_count;
    uint32_t transitions = timer_get_transition_count();
    NRFX_LOG_INFO("[STATS] Samples: %u (+%u/s), Trans: %u",
           current_count,
           samples_since_last,
           transitions);
    last_sample_count = current_count;
}
#endif

int main(void)
{
    nrfx_err_t status;
    
    NRFX_LOG_INFO("=== APP START (DUAL CC CHANNEL MODE) ===");

#if defined(__ZEPHYR__)
    IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_TIMER_INST_GET(TIMER_PULSE_IDX)), IRQ_PRIO_LOWEST,
                NRFX_TIMER_INST_HANDLER_GET(TIMER_PULSE_IDX), 0, 0);
    IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_TIMER_INST_GET(TIMER_STATE_IDX)), IRQ_PRIO_LOWEST,
                NRFX_TIMER_INST_HANDLER_GET(TIMER_STATE_IDX), 0, 0);
    
    
#endif

    NRFX_EXAMPLE_LOG_INIT();
    NRFX_LOG_INFO("=== DUAL CC CHANNEL MUX MODE ===");
    NRFX_LOG_INFO("MUX advance time: %d us", MUX_ADVANCE_TIME_US);

    // ========== DAC INIT ==========
    nrfx_spim_t spim_dac = NRFX_SPIM_INSTANCE(DAC_SPIM_INST_IDX);
    nrfx_err_t dac_err = dac_init(&spim_dac);
    if (dac_err != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("DAC init failed: 0x%08X", dac_err);
        while(1) { k_sleep(K_FOREVER); }
    }

    // ========== SAADC INIT ==========
    status = saadc_init();
    NRFX_ASSERT(status == NRFX_SUCCESS);
    NRFX_LOG_INFO("SAADC initialized");

    // ========== GPIOTE INIT ==========
    status = gpiote_init(&gpiote_ch_pin1, &gpiote_ch_pin2);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    NRFX_LOG_INFO("GPIOTE initialized");

    // ========== GPPI INIT ==========
    status = gppi_init();
    NRFX_ASSERT(status == NRFX_SUCCESS);
    NRFX_LOG_INFO("GPPI channels allocated");

    // ========== TIMER INIT ==========
    uint32_t pulse_us = uart_get_pulse_width_ms() * 100;
    status = timer_init(pulse_us);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    NRFX_LOG_INFO("Timers initialized");

    // ========== MUX INIT ==========
    NRFX_LOG_INFO("Initializing MUX...");
    nrfx_spim_t spim = NRFX_SPIM_INSTANCE(SPIM_INST_IDX);
    nrfx_err_t mux_err = mux_init(&spim);
    if (mux_err != NRFX_SUCCESS) {
        NRFX_LOG_ERROR("MUX init failed: 0x%08X", mux_err);
        while(1) { k_sleep(K_FOREVER); }
    }
    NRFX_LOG_INFO("MUX initialized OK");

 

    // ========== GPPI SETUP ==========
    status = gppi_setup_connections(gpiote_ch_pin1, gpiote_ch_pin2);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    gppi_enable();
    NRFX_LOG_INFO("GPPI connections configured and enabled");

    // Initial MUX pattern for PULSE_1
    mux_write(MUX_PATTERN_PULSE_1);
    mux_wait_ready();  // Wait for completion
    
    // Configure state timer for first pulse
    uint32_t single_pulse_us = pulse_us * 2 + 100;
    timer_set_state_pulse(single_pulse_us);
    
    NRFX_LOG_INFO("Timers enabled with dual CC channels");
    NRFX_LOG_INFO("System started - DUAL CC MODE");

    k_sleep(K_MSEC(100));
    // BLE INIT //
    ble_init();
    // ========== UART INIT ==========
    uart_init();
    uart_start_test_timer(600);
    printk("Ready to receive commands...\n\n");
#if ENABLE_STATS_TIMER
    k_timer_start(&stats_timer, K_SECONDS(1), K_SECONDS(1));
#endif
    
    NRFX_LOG_INFO("=== DUAL CC CHANNEL CONFIGURATION ===");
    NRFX_LOG_INFO("State timer CC0: State transition");
    NRFX_LOG_INFO("State timer CC1: MUX pre-load (%d us advance)", MUX_ADVANCE_TIME_US);
    NRFX_LOG_INFO("Pulse count: %d (LED1 only mode)", NUM_PULSES_PER_CYCLE);
    NRFX_LOG_INFO("=====================================");

    while (1) {
    k_sleep(K_FOREVER); 
}
}