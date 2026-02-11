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
 * NEW MODE: 16 pulses per cycle (configurable)
 * - LED1 toggles for all pulses
 * - LED2 is HIGH during pulse sequence, LOW during pause
 * 
 * This ensures MUX pattern arrives BEFORE the pulse starts!
 */

#include "timer.h"
#include "dac.h"
#include "uart.h"
#include "../../config.h"
#include "../mux/mux.h"
#include "../gpiote/gpiote.h"
#include <nrfx_example.h>
#include <zephyr/kernel.h>
#include <zephyr/irq.h>
#include <hal/nrf_gpio.h>

// Timer instances
static nrfx_timer_t timer_pulse = NRFX_TIMER_INSTANCE(TIMER_PULSE_IDX);
static nrfx_timer_t timer_state = NRFX_TIMER_INSTANCE(TIMER_STATE_IDX);

// MUX patterns for 16 pulses (one pattern per pulse)
// Can be modified via SC command - patterns with value 0 are skipped
static uint16_t mux_patterns[MAX_PULSES_PER_CYCLE] = {
    MUX_PATTERN_PULSE_1,
    MUX_PATTERN_PULSE_2,
    MUX_PATTERN_PULSE_3,
    MUX_PATTERN_PULSE_4,
    MUX_PATTERN_PULSE_5,
    MUX_PATTERN_PULSE_6,
    MUX_PATTERN_PULSE_7,
    MUX_PATTERN_PULSE_8,
    MUX_PATTERN_PULSE_9,
    MUX_PATTERN_PULSE_10,
    MUX_PATTERN_PULSE_11,
    MUX_PATTERN_PULSE_12,
    MUX_PATTERN_PULSE_13,
    MUX_PATTERN_PULSE_14,
    MUX_PATTERN_PULSE_15,
    MUX_PATTERN_PULSE_16
};

/* DAC values corresponding to each pulse (PULSE_1..PULSE_16)
 * These values will be applied before the corresponding pulse
 * (pre-load) so DAC output is ready when the pulse starts.
 * 
 * Can be modified via SA command.
 * Default: Linear ramp from 200 to 4000 across 16 pulses
 */
static uint16_t dac_values[MAX_PULSES_PER_CYCLE] = {
    200,   /* PULSE_1 */
    450,   /* PULSE_2 */
    700,   /* PULSE_3 */
    950,   /* PULSE_4 */
    1200,  /* PULSE_5 */
    1450,  /* PULSE_6 */
    1700,  /* PULSE_7 */
    1950,  /* PULSE_8 */
    2200,  /* PULSE_9 */
    2450,  /* PULSE_10 */
    2700,  /* PULSE_11 */
    2950,  /* PULSE_12 */
    3200,  /* PULSE_13 */
    3450,  /* PULSE_14 */
    3700,  /* PULSE_15 */
    4000   /* PULSE_16 */
};

// Current number of active pulses (can be changed via SC command)
static volatile uint8_t active_pulse_count = NUM_PULSES_PER_CYCLE;

// Current pulse index (0 to active_pulse_count-1)
static volatile uint8_t current_pulse_idx = 0;

// State machine - simplified: PULSE or PAUSE
typedef enum {
    STATE_PULSE,
    STATE_PAUSE
} state_t;

static volatile state_t current_state = STATE_PULSE;
static volatile uint32_t state_transitions = 0;
static volatile bool system_running = true;

static void prepare_outputs_preload_for_current_state(void)
{
    uint16_t pattern;
    uint16_t dac_value;
    uint8_t next_idx;

    if (current_state == STATE_PULSE) {
        next_idx = current_pulse_idx + 1;
        pattern = (next_idx >= active_pulse_count) ? MUX_PATTERN_PAUSE : mux_patterns[next_idx];
        dac_value = (next_idx >= active_pulse_count) ? 0 : dac_values[next_idx];
    } else {
        pattern = mux_patterns[0];
        dac_value = dac_values[0];
    }

    if (mux_prepare_write(pattern) != NRFX_SUCCESS) {
        /* Keep timing deterministic; next CC1 may skip if transfer is still pending. */
    }

    #if ENABLE_DAC_PRELOAD
    if (dac_prepare_value(dac_value) != NRFX_SUCCESS) {
        /* Keep timing deterministic; next CC1 may skip if transfer is still pending. */
    }
    #endif
}

/**
 * @brief State machine timer handler with DUAL CC channels
 * 
 * CC_CHANNEL0: State transition (main event)
 * CC_CHANNEL1: MUX pre-load (early event, MUX_ADVANCE_TIME_US before CC0)
 * 
 * NEW: Simplified state machine with configurable pulse count (1-16)
 */
static void state_timer_handler(nrf_timer_event_t event_type, void * p_context)
{
    // If system is stopped, ignore all events
    if (!system_running) {
        return;
    }
    
    uint32_t pulse_us = uart_get_pulse_width_ms() * 100;
    uint32_t single_pulse_us = pulse_us + PULSE_OVERHEAD_US;
    
    // ========== CC_CHANNEL1: MUX PRE-LOAD EVENT ==========
    if (event_type == NRF_TIMER_EVENT_COMPARE1) {
        return;  // Hardware GPPI triggers MUX and DAC START here.
    }
    
    // ========== CC_CHANNEL0: STATE TRANSITION EVENT ==========
    if (event_type != NRF_TIMER_EVENT_COMPARE0) {
        return;
    }
    
    state_transitions++;
    
    // Check UART parameter updates (atomic test-and-clear)
    if (uart_test_and_clear_update_flag()) {
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
        
        if (current_state == STATE_PULSE) {
            nrfx_timer_enable(&timer_pulse);
        }
    }
    
    // State transitions
    if (current_state == STATE_PULSE) {
        current_pulse_idx++;
        
        if (current_pulse_idx >= active_pulse_count) {
            // All pulses done, go to PAUSE
            nrfx_timer_disable(&timer_pulse);
            current_state = STATE_PAUSE;
            current_pulse_idx = 0;
            
            // LED2 goes LOW during pause
            nrf_gpio_pin_clear(OUTPUT_PIN_2);
            
            // Calculate pause duration
            uint32_t freq_hz = uart_get_frequency_hz();
            uint32_t active_period_us = single_pulse_us * active_pulse_count;
            uint32_t total_period_us = 1000000 / freq_hz;
            uint32_t pause_us = (total_period_us > active_period_us) ? 
                               (total_period_us - active_period_us) : 0;
            
            // Setup state timer for pause
            nrfx_timer_disable(&timer_state);
            nrfx_timer_clear(&timer_state);
            uint32_t pause_ticks = nrfx_timer_us_to_ticks(&timer_state, pause_us);
            
            nrfx_timer_compare(&timer_state, NRF_TIMER_CC_CHANNEL0, pause_ticks, true);
            
            uint32_t advance_ticks = nrfx_timer_us_to_ticks(&timer_state, MUX_ADVANCE_TIME_US);
            uint32_t mux_ticks = (pause_ticks > advance_ticks) ? 
                                 (pause_ticks - advance_ticks) : 
                                 (pause_ticks / 2);
            nrfx_timer_compare(&timer_state, NRF_TIMER_CC_CHANNEL1, mux_ticks, true);
            prepare_outputs_preload_for_current_state();
            nrfx_timer_enable(&timer_state);
        } else {
            // Continue with next pulse
            nrfx_timer_clear(&timer_pulse);
            
            nrfx_timer_disable(&timer_state);
            nrfx_timer_clear(&timer_state);
            uint32_t pulse_ticks = nrfx_timer_us_to_ticks(&timer_state, single_pulse_us);
            
            nrfx_timer_compare(&timer_state, NRF_TIMER_CC_CHANNEL0, pulse_ticks, true);
            
            uint32_t advance_ticks = nrfx_timer_us_to_ticks(&timer_state, MUX_ADVANCE_TIME_US);
            uint32_t mux_ticks = (pulse_ticks > advance_ticks) ? 
                                 (pulse_ticks - advance_ticks) : 
                                 (pulse_ticks / 2);
            nrfx_timer_compare(&timer_state, NRF_TIMER_CC_CHANNEL1, mux_ticks, true);
            prepare_outputs_preload_for_current_state();
            nrfx_timer_enable(&timer_state);
        }
    } else if (current_state == STATE_PAUSE) {
        // After PAUSE, restart with first pulse
        nrfx_timer_enable(&timer_pulse);
        current_state = STATE_PULSE;
        current_pulse_idx = 0;
        
        // LED2 goes HIGH during pulse sequence
        nrf_gpio_pin_set(OUTPUT_PIN_2);
        
        nrfx_timer_disable(&timer_state);
        nrfx_timer_clear(&timer_state);
        uint32_t pulse_ticks = nrfx_timer_us_to_ticks(&timer_state, single_pulse_us);
        
        nrfx_timer_compare(&timer_state, NRF_TIMER_CC_CHANNEL0, pulse_ticks, true);
        
        uint32_t advance_ticks = nrfx_timer_us_to_ticks(&timer_state, MUX_ADVANCE_TIME_US);
        uint32_t mux_ticks = (pulse_ticks > advance_ticks) ? 
                             (pulse_ticks - advance_ticks) : 
                             (pulse_ticks / 2);
        nrfx_timer_compare(&timer_state, NRF_TIMER_CC_CHANNEL1, mux_ticks, true);
        prepare_outputs_preload_for_current_state();
        nrfx_timer_enable(&timer_state);
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
    prepare_outputs_preload_for_current_state();
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
    prepare_outputs_preload_for_current_state();
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
    // CRITICAL: Disable timers FIRST to prevent race condition
    // where ISR could set LED2 HIGH after we clear it
    unsigned int key = irq_lock();
    
    // Disable both timers before changing state
    nrfx_timer_disable(&timer_pulse);
    nrfx_timer_disable(&timer_state);
    
    // Now safe to change state
    system_running = false;
    current_state = STATE_PAUSE;  // Reset to known state
    current_pulse_idx = 0;
    
    irq_unlock(key);
    
    // Set MUX to off/pause pattern
    mux_abort_transfer();
    mux_write(MUX_PATTERN_PAUSE);
    #if ENABLE_DAC_PRELOAD
    dac_abort_transfer();
    dac_set_value(0);
    #endif
    
    // LED2 LOW when system stopped - guaranteed no ISR can change this
    nrf_gpio_pin_clear(OUTPUT_PIN_2);
}

void timer_system_start(void)
{
    if (system_running) {
        return;  // Already running
    }
    
    system_running = true;
    
    // Reset state machine to beginning
    current_state = STATE_PULSE;
    current_pulse_idx = 0;
    
    // LED2 HIGH during pulse sequence
    nrf_gpio_pin_set(OUTPUT_PIN_2);
    
    // Pre-load MUX for first pulse
    mux_abort_transfer();
    mux_write(mux_patterns[0]);
    #if ENABLE_DAC_PRELOAD
    dac_abort_transfer();
    dac_set_value(dac_values[0]);
    #endif
    
    // Get current pulse width
    uint32_t pulse_us = uart_get_pulse_width_ms() * 100;
    uint32_t single_pulse_us = pulse_us + PULSE_OVERHEAD_US;
    
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

void timer_set_pulse_count(uint8_t count)
{
    if (count >= 1 && count <= MAX_PULSES_PER_CYCLE) {
        active_pulse_count = count;
    }
}

uint8_t timer_get_pulse_count(void)
{
    return active_pulse_count;
}

void timer_set_mux_patterns(const uint16_t *patterns, uint8_t count)
{
    if (count > MAX_PULSES_PER_CYCLE) {
        count = MAX_PULSES_PER_CYCLE;
    }
    
    // IRQ lock to prevent race with timer ISR reading mux_patterns[]
    unsigned int key = irq_lock();
    
    // Copy patterns and count non-zero ones
    uint8_t non_zero_count = 0;
    for (uint8_t i = 0; i < MAX_PULSES_PER_CYCLE; i++) {
        if (i < count) {
            mux_patterns[i] = patterns[i];
            if (patterns[i] != 0) {
                non_zero_count = i + 1;  // Last non-zero index + 1
            }
        } else {
            mux_patterns[i] = 0;
        }
    }
    
    // Set active pulse count to number of consecutive non-zero patterns from start
    // Actually, count all until we hit the last non-zero
    active_pulse_count = (non_zero_count > 0) ? non_zero_count : 1;
    
    irq_unlock(key);
}

uint16_t timer_get_mux_pattern(uint8_t index)
{
    if (index < MAX_PULSES_PER_CYCLE) {
        return mux_patterns[index];
    }
    return 0;
}

void timer_set_dac_values(const uint16_t *values, uint8_t count)
{
    if (count > MAX_PULSES_PER_CYCLE) {
        count = MAX_PULSES_PER_CYCLE;
    }
    
    // IRQ lock to prevent race with timer ISR reading dac_values[]
    unsigned int key = irq_lock();
    
    // Copy DAC values and clear the remaining slots to zero.
    for (uint8_t i = 0; i < MAX_PULSES_PER_CYCLE; i++) {
        if (i < count) {
            dac_values[i] = values[i];
        } else {
            dac_values[i] = 0;
        }
    }
    
    irq_unlock(key);
}

uint16_t timer_get_dac_value(uint8_t index)
{
    if (index < MAX_PULSES_PER_CYCLE) {
        return dac_values[index];
    }
    return 0;
}

uint32_t timer_get_single_pulse_us(void)
{
    uint32_t pulse_us = uart_get_pulse_width_ms() * 100;
    return pulse_us + PULSE_OVERHEAD_US;
}

uint32_t timer_get_active_time_us(void)
{
    return timer_get_single_pulse_us() * active_pulse_count;
}

uint32_t timer_get_max_frequency_hz(void)
{
    uint32_t active_time_us = timer_get_active_time_us();
    // Dodaj minimalnu pauzu od 100us
    uint32_t min_period_us = active_time_us + 100;
    return 1000000U / min_period_us;
}
