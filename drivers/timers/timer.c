/**
 * @file timer.c
 * @brief Timer driver implementation for dual-timer pulse generation
 * 
 * ULTRA LOW POWER VERSION with DUAL CC CHANNEL MUX PRE-LOADING
 * 
 * State timer uses TWO compare channels:
 * - CC_CHANNEL0: State transition event (main timing)
 * - CC_CHANNEL1: MUX pre-load event (MUX_ADVANCE_TIME_US before state transition)
 * 
 * This ensures MUX pattern arrives BEFORE the pulse starts!
 */

#include "timer.h"
#include "dac.h"
#include "uart.h"
#include "../../config.h"
#include "../mux/mux.h"
#include <nrfx_example.h>
#include <zephyr/kernel.h>

// Timer instances
static nrfx_timer_t timer_pulse = NRFX_TIMER_INSTANCE(TIMER_PULSE_IDX);
static nrfx_timer_t timer_state = NRFX_TIMER_INSTANCE(TIMER_STATE_IDX);

// MUX patterns for 8 pulses (one pattern per pulse)
static const uint16_t mux_patterns[8] = {
    MUX_PATTERN_PULSE_1,
    MUX_PATTERN_PULSE_2,
    MUX_PATTERN_PULSE_3,
    MUX_PATTERN_PULSE_4,
    MUX_PATTERN_PULSE_5,
    MUX_PATTERN_PULSE_6,
    MUX_PATTERN_PULSE_7,
    MUX_PATTERN_PULSE_8
};

/* DAC values corresponding to each pulse (PULSE_1..PULSE_8)
 * These values will be applied before the corresponding pulse
 * (pre-load) so DAC output is ready when the pulse starts.
 */
static const uint16_t dac_values[8] = {
    500,  /* PULSE_1 */
    800,  /* PULSE_2 */
    1200, /* PULSE_3 */
    1600, /* PULSE_4 */
    2000, /* PULSE_5 */
    2400, /* PULSE_6 */
    2800, /* PULSE_7 */
    3200  /* PULSE_8 */
};

// State machine
typedef enum {
    STATE_PULSE_1,
    STATE_PULSE_2,
    STATE_PULSE_3,
    STATE_PULSE_4,
    STATE_PULSE_5,
    STATE_PULSE_6,
    STATE_PULSE_7,
    STATE_PULSE_8,
    STATE_PAUSE
} state_t;

static volatile state_t current_state = STATE_PULSE_1;
static volatile uint32_t state_transitions = 0;
static volatile bool system_running = true;

/**
 * @brief State machine timer handler with DUAL CC channels
 * 
 * CC_CHANNEL0: State transition (main event)
 * CC_CHANNEL1: MUX pre-load (early event, MUX_ADVANCE_TIME_US before CC0)
 */
static void state_timer_handler(nrf_timer_event_t event_type, void * p_context)
{
    // If system is stopped, ignore all events
    if (!system_running) {
        return;
    }
    
    uint32_t pulse_us = uart_get_pulse_width_ms() * 100;
    uint32_t single_pulse_us = pulse_us * 2 + PULSE_OVERHEAD_US;
    
    // ========== CC_CHANNEL1: MUX PRE-LOAD EVENT ==========
    if (event_type == NRF_TIMER_EVENT_COMPARE1) {
        // Send MUX pattern for NEXT state (before state actually transitions)
        switch(current_state) {
            case STATE_PULSE_1:
                /* Pre-load for PULSE_2 */
                mux_write(mux_patterns[1]);
                #if ENABLE_DAC_PRELOAD
                    dac_set_value(dac_values[1]);
                #endif
                break;
            case STATE_PULSE_2:
                /* Pre-load for PULSE_3 */
                mux_write(mux_patterns[2]);
                #if ENABLE_DAC_PRELOAD
                    dac_set_value(dac_values[2]);
                #endif
                break;
            case STATE_PULSE_3:
                /* Pre-load for PULSE_4 */
                mux_write(mux_patterns[3]);
                #if ENABLE_DAC_PRELOAD
                    dac_set_value(dac_values[3]);
                #endif
                break;
            case STATE_PULSE_4:
                /* Pre-load for PULSE_5 */
                mux_write(mux_patterns[4]);
                #if ENABLE_DAC_PRELOAD
                    dac_set_value(dac_values[4]);
                #endif
                break;
            case STATE_PULSE_5:
                /* Pre-load for PULSE_6 */
                mux_write(mux_patterns[5]);
                #if ENABLE_DAC_PRELOAD
                    dac_set_value(dac_values[5]);
                #endif
                break;
            case STATE_PULSE_6:
                /* Pre-load for PULSE_7 */
                mux_write(mux_patterns[6]);
                #if ENABLE_DAC_PRELOAD
                    dac_set_value(dac_values[6]);
                #endif
                break;
            case STATE_PULSE_7:
                /* Pre-load for PULSE_8 */
                mux_write(mux_patterns[7]);
                #if ENABLE_DAC_PRELOAD
                    dac_set_value(dac_values[7]);
                #endif
                break;
            case STATE_PULSE_8:
                /* Pre-load for PAUSE (all off) */
                mux_write(MUX_PATTERN_PAUSE);
                #if ENABLE_DAC_PRELOAD
                    /* Set DAC to 0 during pause */
                    dac_set_value(0);
                #endif
                break;
            case STATE_PAUSE:
                /* Pre-load for PULSE_1 (restart) */
                mux_write(mux_patterns[0]);
                #if ENABLE_DAC_PRELOAD
                    dac_set_value(dac_values[0]);
                #endif
                break;
        }
        return;  // Only send pattern, don't change state
    }
    
    // ========== CC_CHANNEL0: STATE TRANSITION EVENT ==========
    if (event_type != NRF_TIMER_EVENT_COMPARE0) {
        return;
    }
    
    state_transitions++;
    
    // Check UART parameter updates (UART provides the parameter API)
    if (uart_parameters_updated()) {
        uart_clear_update_flag();

        uint32_t new_pulse_us = uart_get_pulse_width_ms() * 100;
        
        nrfx_timer_disable(&timer_pulse);
        nrfx_timer_clear(&timer_pulse);
        
        uint32_t pulse_ticks = nrfx_timer_us_to_ticks(&timer_pulse, new_pulse_us);
        nrfx_timer_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL0, 10, false);
        nrfx_timer_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL1, pulse_ticks + 10, false);
        nrfx_timer_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL2, pulse_ticks + 20, false);
        nrfx_timer_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL3, pulse_ticks * 2 + 20, false);
        nrfx_timer_extended_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL5,
                                    pulse_ticks * 2 + 30,
                                    NRF_TIMER_SHORT_COMPARE5_CLEAR_MASK, false);
        
        if (current_state >= STATE_PULSE_1 && current_state <= STATE_PULSE_8) {
            nrfx_timer_enable(&timer_pulse);
        }
    }
    
    // State transitions
    switch(current_state) {
        case STATE_PULSE_1:
        case STATE_PULSE_2:
        case STATE_PULSE_3:
        case STATE_PULSE_4:
        case STATE_PULSE_5:
        case STATE_PULSE_6:
        case STATE_PULSE_7: {
            // Move to next pulse state
            nrfx_timer_clear(&timer_pulse);
            current_state = (state_t)(current_state + 1);
            
            nrfx_timer_disable(&timer_state);
            nrfx_timer_clear(&timer_state);
            uint32_t pulse_ticks = nrfx_timer_us_to_ticks(&timer_state, single_pulse_us);
            
            // Setup CC0 for state transition
            nrfx_timer_compare(&timer_state, NRF_TIMER_CC_CHANNEL0, pulse_ticks, true);
            
            // Setup CC1 for MUX pre-load (ADVANCE_TIME before CC0)
            uint32_t advance_ticks = nrfx_timer_us_to_ticks(&timer_state, MUX_ADVANCE_TIME_US);
            uint32_t mux_ticks = (pulse_ticks > advance_ticks) ? 
                                 (pulse_ticks - advance_ticks) : 
                                 (pulse_ticks / 2);
            nrfx_timer_compare(&timer_state, NRF_TIMER_CC_CHANNEL1, mux_ticks, true);
            
            nrfx_timer_enable(&timer_state);
            break;
        }
        
        case STATE_PULSE_8: {
            // After 8th pulse, go to PAUSE
            nrfx_timer_disable(&timer_pulse);
            current_state = STATE_PAUSE;
            
            uint32_t freq_hz = uart_get_frequency_hz();
            uint32_t active_period_us = single_pulse_us * 8;
            uint32_t total_period_us = 1000000 / freq_hz;
            uint32_t pause_us = (total_period_us > active_period_us) ? 
                               (total_period_us - active_period_us) : 0;
            
            nrfx_timer_disable(&timer_state);
            nrfx_timer_clear(&timer_state);
            uint32_t pause_ticks = nrfx_timer_us_to_ticks(&timer_state, pause_us);
            
            // Setup CC0 for state transition
            nrfx_timer_compare(&timer_state, NRF_TIMER_CC_CHANNEL0, pause_ticks, true);
            
            // Setup CC1 for MUX pre-load
            uint32_t advance_ticks = nrfx_timer_us_to_ticks(&timer_state, MUX_ADVANCE_TIME_US);
            uint32_t mux_ticks = (pause_ticks > advance_ticks) ? 
                                 (pause_ticks - advance_ticks) : 
                                 (pause_ticks / 2);
            nrfx_timer_compare(&timer_state, NRF_TIMER_CC_CHANNEL1, mux_ticks, true);
            
            nrfx_timer_enable(&timer_state);
            break;
        }
        
        case STATE_PAUSE: {
            // After PAUSE, back to PULSE_1
            nrfx_timer_enable(&timer_pulse);
            current_state = STATE_PULSE_1;
            
            nrfx_timer_disable(&timer_state);
            nrfx_timer_clear(&timer_state);
            uint32_t pulse_ticks = nrfx_timer_us_to_ticks(&timer_state, single_pulse_us);
            
            // Setup CC0 for state transition
            nrfx_timer_compare(&timer_state, NRF_TIMER_CC_CHANNEL0, pulse_ticks, true);
            
            // Setup CC1 for MUX pre-load
            uint32_t advance_ticks = nrfx_timer_us_to_ticks(&timer_state, MUX_ADVANCE_TIME_US);
            uint32_t mux_ticks = (pulse_ticks > advance_ticks) ? 
                                 (pulse_ticks - advance_ticks) : 
                                 (pulse_ticks / 2);
            nrfx_timer_compare(&timer_state, NRF_TIMER_CC_CHANNEL1, mux_ticks, true);
            
            nrfx_timer_enable(&timer_state);
            break;
        }
    }
}

nrfx_err_t timer_init(uint32_t pulse_width_us)
{
    nrfx_err_t status;
    
    // ========== PULSE TIMER ==========
    uint32_t base_freq_pulse = NRF_TIMER_BASE_FREQUENCY_GET(timer_pulse.p_reg);
    nrfx_timer_config_t pulse_config = NRFX_TIMER_DEFAULT_CONFIG(base_freq_pulse);
    pulse_config.bit_width = NRF_TIMER_BIT_WIDTH_32;

    status = nrfx_timer_init(&timer_pulse, &pulse_config, NULL);
    if (status != NRFX_SUCCESS) {
        return status;
    }
    
    // Configure pulse timer compare channels
    nrfx_timer_disable(&timer_pulse);
    nrfx_timer_clear(&timer_pulse);
    
    uint32_t pulse_ticks = nrfx_timer_us_to_ticks(&timer_pulse, pulse_width_us);
    
    nrfx_timer_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL0, 10, false);
    nrfx_timer_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL1, pulse_ticks + 10, false);
    nrfx_timer_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL2, pulse_ticks + 20, false);
    nrfx_timer_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL3, pulse_ticks * 2 + 20, false);
    nrfx_timer_extended_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL5,
                                pulse_ticks * 2 + 30,
                                NRF_TIMER_SHORT_COMPARE5_CLEAR_MASK, false);
    
    nrfx_timer_enable(&timer_pulse);

    // ========== STATE TIMER ==========
    uint32_t base_freq_state = NRF_TIMER_BASE_FREQUENCY_GET(timer_state.p_reg);
    nrfx_timer_config_t state_config = NRFX_TIMER_DEFAULT_CONFIG(base_freq_state);
    state_config.bit_width = NRF_TIMER_BIT_WIDTH_32;

    status = nrfx_timer_init(&timer_state, &state_config, state_timer_handler);
    if (status != NRFX_SUCCESS) {
        return status;
    }
    
    return NRFX_SUCCESS;
}

void timer_update_pulse_width(uint32_t pulse_width_us)
{
    nrfx_timer_disable(&timer_pulse);
    nrfx_timer_clear(&timer_pulse);
    
    uint32_t pulse_ticks = nrfx_timer_us_to_ticks(&timer_pulse, pulse_width_us);
    
    nrfx_timer_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL0, 10, false);
    nrfx_timer_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL1, pulse_ticks + 10, false);
    nrfx_timer_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL2, pulse_ticks + 20, false);
    nrfx_timer_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL3, pulse_ticks * 2 + 20, false);
    nrfx_timer_extended_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL5,
                                pulse_ticks * 2 + 30,
                                NRF_TIMER_SHORT_COMPARE5_CLEAR_MASK, false);
    
    nrfx_timer_enable(&timer_pulse);
}

void timer_set_state_pulse(uint32_t single_pulse_us)
{
    nrfx_timer_disable(&timer_state);
    nrfx_timer_clear(&timer_state);
    
    uint32_t pulse_ticks = nrfx_timer_us_to_ticks(&timer_state, single_pulse_us);
    
    // Setup CC0 for state transition
    nrfx_timer_compare(&timer_state, NRF_TIMER_CC_CHANNEL0, pulse_ticks, true);
    
    // Setup CC1 for MUX pre-load (ADVANCE_TIME before CC0)
    uint32_t advance_ticks = nrfx_timer_us_to_ticks(&timer_state, MUX_ADVANCE_TIME_US);
    uint32_t mux_ticks = (pulse_ticks > advance_ticks) ? 
                         (pulse_ticks - advance_ticks) : 
                         (pulse_ticks / 2);
    nrfx_timer_compare(&timer_state, NRF_TIMER_CC_CHANNEL1, mux_ticks, true);
    
    nrfx_timer_enable(&timer_state);
}

void timer_set_state_pause(uint32_t pause_us)
{
    nrfx_timer_disable(&timer_state);
    nrfx_timer_clear(&timer_state);
    
    uint32_t pause_ticks = nrfx_timer_us_to_ticks(&timer_state, pause_us);
    
    // Setup CC0 for state transition
    nrfx_timer_compare(&timer_state, NRF_TIMER_CC_CHANNEL0, pause_ticks, true);
    
    // Setup CC1 for MUX pre-load
    uint32_t advance_ticks = nrfx_timer_us_to_ticks(&timer_state, MUX_ADVANCE_TIME_US);
    uint32_t mux_ticks = (pause_ticks > advance_ticks) ? 
                         (pause_ticks - advance_ticks) : 
                         (pause_ticks / 2);
    nrfx_timer_compare(&timer_state, NRF_TIMER_CC_CHANNEL1, mux_ticks, true);
    
    nrfx_timer_enable(&timer_state);
}

void timer_pulse_enable(bool enable)
{
    if (enable) {
        nrfx_timer_enable(&timer_pulse);
    } else {
        nrfx_timer_disable(&timer_pulse);
    }
}

void timer_get_instances(nrfx_timer_t **pulse, nrfx_timer_t **state)
{
    if (pulse) {
        *pulse = &timer_pulse;
    }
    if (state) {
        *state = &timer_state;
    }
}

uint32_t timer_get_transition_count(void)
{
    return state_transitions;
}

void timer_system_stop(void)
{
    system_running = false;
    
    // Disable both timers
    nrfx_timer_disable(&timer_pulse);
    nrfx_timer_disable(&timer_state);
    
    // Set MUX to off/pause pattern
    mux_write(MUX_PATTERN_PAUSE);
}

void timer_system_start(void)
{
    if (system_running) {
        return;  // Already running
    }
    
    system_running = true;
    
    // Reset state machine to beginning
    current_state = STATE_PULSE_1;
    
    // Pre-load MUX for first pulse
    mux_write(mux_patterns[0]);
    
    // Get current pulse width
    uint32_t pulse_us = uart_get_pulse_width_ms() * 100;
    uint32_t single_pulse_us = pulse_us * 2 + PULSE_OVERHEAD_US;
    
    // Re-enable pulse timer
    nrfx_timer_clear(&timer_pulse);
    nrfx_timer_enable(&timer_pulse);
    
    // Configure and enable state timer for first pulse
    timer_set_state_pulse(single_pulse_us);
}

bool timer_system_is_running(void)
{
    return system_running;
}
