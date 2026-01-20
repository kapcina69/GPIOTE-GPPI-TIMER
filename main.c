/*
 * ULTRA LOW POWER VERSION
 * Minimizes CPU wake-ups by:
 * 1. Batching ADC interrupts (only every Nth sample)
 * 2. Disabling logging in interrupts
 * 3. Conditional stats timer (only when needed)
 * 4. Efficient state transitions
 */

#include <nrfx_example.h>
#include <helpers/nrfx_gppi.h>
#include <nrfx_timer.h>
#include <nrfx_gpiote.h>
#include <nrfx_saadc.h>
#include <hal/nrf_saadc.h>
#include "ble.h"

#define NRFX_LOG_MODULE                 EXAMPLE
#define NRFX_EXAMPLE_CONFIG_LOG_ENABLED 1
#define NRFX_EXAMPLE_CONFIG_LOG_LEVEL   3
#include <nrfx_log.h>

// Timer instances
#define TIMER_PULSE_IDX 1
#define TIMER_STATE_IDX 2
#define GPIOTE_INST_IDX 0

// GPIO pins
#define OUTPUT_PIN_1 LED1_PIN
#define OUTPUT_PIN_2 LED2_PIN

// SAADC config
#define SAADC_CHANNEL_AIN NRF_SAADC_INPUT_AIN0
#define SAADC_RESOLUTION NRF_SAADC_RESOLUTION_10BIT

// CRITICAL: ADC interrupt batching - only wake CPU every N samples
#define ADC_INTERRUPT_BATCH_SIZE 10  // Wake CPU only every 10th sample
#define LOG_EVERY_N_SAMPLES 100     // Rare logging

// Enable/disable features for power testing
#define ENABLE_STATS_TIMER 0         // Set to 0 to disable periodic stats
#define ENABLE_ADC_LOGGING 1         // Set to 0 to disable ADC voltage logging

// Timer instances
static nrfx_timer_t timer_pulse = NRFX_TIMER_INSTANCE(TIMER_PULSE_IDX);
static nrfx_timer_t timer_state = NRFX_TIMER_INSTANCE(TIMER_STATE_IDX);
static nrfx_gpiote_t const *p_gpiote_inst = NULL;

// GPPI channels
static uint8_t gppi_pin1_set;
static uint8_t gppi_pin1_clr;
static uint8_t gppi_pin2_set;
static uint8_t gppi_pin2_clr;
static uint8_t gppi_adc_trigger;
static uint8_t gppi_adc_capture;

// GPIOTE channels
static uint8_t gpiote_ch_pin1;
static uint8_t gpiote_ch_pin2;

// ADC variables - optimized for batching
static int16_t m_saadc_buffer[ADC_INTERRUPT_BATCH_SIZE];  // Batch buffer
static int16_t m_saadc_latest_sample;
static volatile uint32_t sample_counter = 0;

// Stats (conditional compilation)
#if ENABLE_STATS_TIMER
static void stats_timer_callback(struct k_timer *timer);
K_TIMER_DEFINE(stats_timer, stats_timer_callback, NULL);
static uint32_t last_sample_count = 0;
#endif

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

/**
 * @brief Convert ADC sample to millivolts
 */
static inline int32_t saadc_sample_to_mv(int16_t sample)
{
    return ((int32_t)sample * 3600) / 1024;
}

/**
 * @brief SAADC event handler - OPTIMIZED for minimal wake-ups
 * 
 * Strategy:
 * - Use BATCH buffer (10+ samples)
 * - Only wake CPU when buffer is FULL
 * - Minimal processing in interrupt
 * - No logging in interrupt (unless debug needed)
 */
static void saadc_handler(nrfx_saadc_evt_t const * p_event)
{
    switch (p_event->type)
    {
        case NRFX_SAADC_EVT_BUF_REQ:
            // Just provide next buffer - minimal work
            nrfx_saadc_buffer_set(m_saadc_buffer, ADC_INTERRUPT_BATCH_SIZE);
            break;

        case NRFX_SAADC_EVT_DONE:
            // CRITICAL: This fires only when BATCH is complete (every 10 samples)
            // Extract last sample from batch
            m_saadc_latest_sample = NRFX_SAADC_SAMPLE_GET(
                SAADC_RESOLUTION, 
                p_event->data.done.p_buffer, 
                ADC_INTERRUPT_BATCH_SIZE - 1  // Last sample in batch
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
            // Re-arm for next batch
            nrfx_saadc_buffer_set(m_saadc_buffer, ADC_INTERRUPT_BATCH_SIZE);
            nrfx_saadc_mode_trigger();
            break;

        default:
            break;
    }
}

#if ENABLE_STATS_TIMER
/**
 * @brief Periodic stats output (conditional)
 */
static void stats_timer_callback(struct k_timer *timer)
{
    uint32_t samples_since_last = sample_counter - last_sample_count;
    printk("[STATS] State: %s, Samples: %u (+%u/s), Trans: %u\n",
           current_state == STATE_ACTIVE ? "ACT" : "PAU",
           sample_counter,
           samples_since_last,
           state_transitions);
    last_sample_count = sample_counter;
}
#endif

/**
 * @brief Setup pulse generation timer
 */
static void setup_pulse_timer(uint32_t pulse_us)
{
    nrfx_timer_disable(&timer_pulse);
    nrfx_timer_clear(&timer_pulse);
    
    uint32_t pulse_ticks = nrfx_timer_us_to_ticks(&timer_pulse, pulse_us);
    
    nrfx_timer_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL0, 10, false);
    nrfx_timer_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL1, pulse_ticks + 10, false);
    nrfx_timer_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL2, pulse_ticks + 20, false);
    nrfx_timer_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL3, pulse_ticks * 2 + 20, false);
    nrfx_timer_extended_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL5,
                                pulse_ticks * 2 + 30,
                                NRF_TIMER_SHORT_COMPARE5_CLEAR_MASK, false);
    
    nrfx_timer_enable(&timer_pulse);
}

/**
 * @brief Setup GPPI connections
 */
static void setup_gppi_connections(void)
{
    uint32_t pin1_set_addr = (uint32_t)&NRF_GPIOTE->TASKS_SET[gpiote_ch_pin1];
    uint32_t pin1_clr_addr = (uint32_t)&NRF_GPIOTE->TASKS_CLR[gpiote_ch_pin1];
    uint32_t pin2_set_addr = (uint32_t)&NRF_GPIOTE->TASKS_SET[gpiote_ch_pin2];
    uint32_t pin2_clr_addr = (uint32_t)&NRF_GPIOTE->TASKS_CLR[gpiote_ch_pin2];
    
    uint32_t timer_cc0_event = nrfx_timer_compare_event_address_get(&timer_pulse, NRF_TIMER_CC_CHANNEL0);
    uint32_t timer_cc1_event = nrfx_timer_compare_event_address_get(&timer_pulse, NRF_TIMER_CC_CHANNEL1);
    uint32_t timer_cc2_event = nrfx_timer_compare_event_address_get(&timer_pulse, NRF_TIMER_CC_CHANNEL2);
    uint32_t timer_cc3_event = nrfx_timer_compare_event_address_get(&timer_pulse, NRF_TIMER_CC_CHANNEL3);
    
    uint32_t saadc_sample_task = nrf_saadc_task_address_get(NRF_SAADC, NRF_SAADC_TASK_SAMPLE);
    uint32_t saadc_end_event = nrf_saadc_event_address_get(NRF_SAADC, NRF_SAADC_EVENT_END);
    uint32_t timer_capture_task = nrfx_timer_task_address_get(&timer_pulse, NRF_TIMER_TASK_CAPTURE4);
    
    nrfx_gppi_channel_endpoints_setup(gppi_pin1_set, timer_cc0_event, pin1_clr_addr);
    nrfx_gppi_channel_endpoints_setup(gppi_pin1_clr, timer_cc1_event, pin1_set_addr);
    nrfx_gppi_channel_endpoints_setup(gppi_pin2_set, timer_cc2_event, pin2_clr_addr);
    nrfx_gppi_channel_endpoints_setup(gppi_pin2_clr, timer_cc3_event, pin2_set_addr);
    nrfx_gppi_channel_endpoints_setup(gppi_adc_trigger, timer_cc1_event, saadc_sample_task);
    nrfx_gppi_channel_endpoints_setup(gppi_adc_capture, saadc_end_event, timer_capture_task);
    
    uint32_t channels_mask = BIT(gppi_pin1_set) | BIT(gppi_pin1_clr) |
                             BIT(gppi_pin2_set) | BIT(gppi_pin2_clr) |
                             BIT(gppi_adc_trigger) | BIT(gppi_adc_capture);
    
    nrfx_gppi_channels_enable(channels_mask);
}

/**
 * @brief State machine timer handler - OPTIMIZED
 * 
 * Minimizes wake-ups by:
 * - Only waking for state transitions
 * - Batching BLE parameter updates
 * - No logging unless necessary
 */
static void state_timer_handler(nrf_timer_event_t event_type, void * p_context)
{
    if (event_type != NRF_TIMER_EVENT_COMPARE0) {
        return;
    }
    
    state_transitions++;
    
    // Check BLE updates (minimal processing)
    if (ble_parameters_updated()) {
        ble_clear_update_flag();
        
        uint32_t new_pulse_us = ble_get_pulse_width_ms() * 100;
        
        // Reconfigure pulse timer
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
    
    // State transitions (minimal work) - 8 pulse states + 1 pause state
    uint32_t pulse_us = ble_get_pulse_width_ms() * 100;
    uint32_t single_pulse_us = pulse_us * 2 + 100;
    
    switch(current_state) {
        case STATE_PULSE_1:
        case STATE_PULSE_2:
        case STATE_PULSE_3:
        case STATE_PULSE_4:
        case STATE_PULSE_5:
        case STATE_PULSE_6:
        case STATE_PULSE_7: {
            // Reset TIMER1 for next pulse and go to next pulse state
            nrfx_timer_clear(&timer_pulse);
            current_state = (state_t)(current_state + 1);
            
            nrfx_timer_disable(&timer_state);
            nrfx_timer_clear(&timer_state);
            uint32_t pulse_ticks = nrfx_timer_us_to_ticks(&timer_state, single_pulse_us);
            nrfx_timer_compare(&timer_state, NRF_TIMER_CC_CHANNEL0, pulse_ticks, true);
            nrfx_timer_enable(&timer_state);
            break;
        }
        
        case STATE_PULSE_8: {
            // After 8th pulse, go to PAUSE
            nrfx_timer_disable(&timer_pulse);
            current_state = STATE_PAUSE;
            
            uint32_t freq_hz = ble_get_frequency_hz();
            uint32_t active_period_us = single_pulse_us * 8;  // 8 pulse sequences
            uint32_t total_period_us = 1000000 / freq_hz;
            uint32_t pause_us = (total_period_us > active_period_us) ? 
                               (total_period_us - active_period_us) : 0;
            
            nrfx_timer_disable(&timer_state);
            nrfx_timer_clear(&timer_state);
            uint32_t pause_ticks = nrfx_timer_us_to_ticks(&timer_state, pause_us);
            nrfx_timer_compare(&timer_state, NRF_TIMER_CC_CHANNEL0, pause_ticks, true);
            nrfx_timer_enable(&timer_state);
            break;
        }
        
        case STATE_PAUSE: {
            nrfx_timer_enable(&timer_pulse);
            current_state = STATE_PULSE_1;
            
            nrfx_timer_disable(&timer_state);
            nrfx_timer_clear(&timer_state);
            uint32_t pulse_ticks = nrfx_timer_us_to_ticks(&timer_state, single_pulse_us);
            nrfx_timer_compare(&timer_state, NRF_TIMER_CC_CHANNEL0, pulse_ticks, true);
            nrfx_timer_enable(&timer_state);
            break;
        }
    }
}

int main(void)
{
    nrfx_err_t status;

#if defined(__ZEPHYR__)
    IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_TIMER_INST_GET(TIMER_PULSE_IDX)), IRQ_PRIO_LOWEST,
                NRFX_TIMER_INST_HANDLER_GET(TIMER_PULSE_IDX), 0, 0);
    IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_TIMER_INST_GET(TIMER_STATE_IDX)), IRQ_PRIO_LOWEST,
                NRFX_TIMER_INST_HANDLER_GET(TIMER_STATE_IDX), 0, 0);
    IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_GPIOTE_INST_GET(GPIOTE_INST_IDX)), IRQ_PRIO_LOWEST,
                NRFX_GPIOTE_INST_HANDLER_GET(GPIOTE_INST_IDX), 0, 0);
    IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_SAADC), IRQ_PRIO_LOWEST, 
                nrfx_saadc_irq_handler, 0, 0);
#endif

    NRFX_EXAMPLE_LOG_INIT();
    NRFX_LOG_INFO("=== ULTRA LOW POWER MODE ===");
    NRFX_LOG_INFO("ADC batch size: %d samples", ADC_INTERRUPT_BATCH_SIZE);
    NRFX_LOG_INFO("Stats timer: %s", ENABLE_STATS_TIMER ? "ON" : "OFF");

    // ========== SAADC INIT (BATCHED) ==========
    status = nrfx_saadc_init(NRFX_SAADC_DEFAULT_CONFIG_IRQ_PRIORITY);
    NRFX_ASSERT(status == NRFX_SUCCESS);

    status = nrfx_saadc_channel_config(&m_saadc_channel);
    NRFX_ASSERT(status == NRFX_SUCCESS);

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
    NRFX_ASSERT(status == NRFX_SUCCESS);

    // Use BATCH buffer
    status = nrfx_saadc_buffer_set(m_saadc_buffer, ADC_INTERRUPT_BATCH_SIZE);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    
    nrf_saadc_enable(NRF_SAADC);
    status = nrfx_saadc_mode_trigger();
    NRFX_ASSERT(status == NRFX_SUCCESS);
    
    NRFX_LOG_INFO("SAADC batched mode initialized");

    // ========== GPIOTE INIT ==========
    nrfx_gpiote_t const gpiote_inst = NRFX_GPIOTE_INSTANCE(GPIOTE_INST_IDX);
    p_gpiote_inst = &gpiote_inst;
    
    status = nrfx_gpiote_init(&gpiote_inst, NRFX_GPIOTE_DEFAULT_CONFIG_IRQ_PRIORITY);
    NRFX_ASSERT(status == NRFX_SUCCESS);

    status = nrfx_gpiote_channel_alloc(&gpiote_inst, &gpiote_ch_pin1);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    status = nrfx_gpiote_channel_alloc(&gpiote_inst, &gpiote_ch_pin2);
    NRFX_ASSERT(status == NRFX_SUCCESS);

    static const nrfx_gpiote_output_config_t output_config = {
        .drive = NRF_GPIO_PIN_S0S1,
        .input_connect = NRF_GPIO_PIN_INPUT_DISCONNECT,
        .pull = NRF_GPIO_PIN_NOPULL,
    };

    const nrfx_gpiote_task_config_t task_config_pin1 = {
        .task_ch = gpiote_ch_pin1,
        .polarity = NRF_GPIOTE_POLARITY_LOTOHI,
        .init_val = NRF_GPIOTE_INITIAL_VALUE_HIGH,
    };

    status = nrfx_gpiote_output_configure(&gpiote_inst, OUTPUT_PIN_1,
                                          &output_config, &task_config_pin1);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    nrfx_gpiote_out_task_enable(&gpiote_inst, OUTPUT_PIN_1);

    const nrfx_gpiote_task_config_t task_config_pin2 = {
        .task_ch = gpiote_ch_pin2,
        .polarity = NRF_GPIOTE_POLARITY_LOTOHI,
        .init_val = NRF_GPIOTE_INITIAL_VALUE_HIGH,
    };

    status = nrfx_gpiote_output_configure(&gpiote_inst, OUTPUT_PIN_2,
                                          &output_config, &task_config_pin2);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    nrfx_gpiote_out_task_enable(&gpiote_inst, OUTPUT_PIN_2);

    // ========== GPPI ALLOCATION ==========
    status = nrfx_gppi_channel_alloc(&gppi_pin1_set);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    status = nrfx_gppi_channel_alloc(&gppi_pin1_clr);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    status = nrfx_gppi_channel_alloc(&gppi_pin2_set);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    status = nrfx_gppi_channel_alloc(&gppi_pin2_clr);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    status = nrfx_gppi_channel_alloc(&gppi_adc_trigger);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    status = nrfx_gppi_channel_alloc(&gppi_adc_capture);
    NRFX_ASSERT(status == NRFX_SUCCESS);

    // ========== PULSE TIMER ==========
    uint32_t base_freq_pulse = NRF_TIMER_BASE_FREQUENCY_GET(timer_pulse.p_reg);
    nrfx_timer_config_t pulse_config = NRFX_TIMER_DEFAULT_CONFIG(base_freq_pulse);
    pulse_config.bit_width = NRF_TIMER_BIT_WIDTH_32;

    status = nrfx_timer_init(&timer_pulse, &pulse_config, NULL);
    NRFX_ASSERT(status == NRFX_SUCCESS);

    // ========== STATE TIMER ==========
    uint32_t base_freq_state = NRF_TIMER_BASE_FREQUENCY_GET(timer_state.p_reg);
    nrfx_timer_config_t state_config = NRFX_TIMER_DEFAULT_CONFIG(base_freq_state);
    state_config.bit_width = NRF_TIMER_BIT_WIDTH_32;

    status = nrfx_timer_init(&timer_state, &state_config, state_timer_handler);
    NRFX_ASSERT(status == NRFX_SUCCESS);

    // ========== SETUP ==========
    uint32_t pulse_us = ble_get_pulse_width_ms() * 100;
    
    setup_pulse_timer(pulse_us);
    setup_gppi_connections();

    current_state = STATE_PULSE_1;
    uint32_t active_us = pulse_us * 2 + 100;  // First pulse duration
    uint32_t active_ticks = nrfx_timer_us_to_ticks(&timer_state, active_us);
    
    nrfx_timer_compare(&timer_state, NRF_TIMER_CC_CHANNEL0, active_ticks, true);
    nrfx_timer_enable(&timer_state);
    
    NRFX_LOG_INFO("System started - LOW POWER MODE");

    // ========== BLE INIT ==========
    int ble_err = ble_init();
    if (ble_err) {
        NRFX_LOG_ERROR("BLE init failed: %d", ble_err);
    }

#if ENABLE_STATS_TIMER
    k_timer_start(&stats_timer, K_SECONDS(1), K_SECONDS(1));
    NRFX_LOG_INFO("Stats timer enabled");
#endif
    
    printk("\n=== LOW POWER CONFIGURATION ===\n");
    printk("ADC interrupt batching: %d samples\n", ADC_INTERRUPT_BATCH_SIZE);
    printk("Stats timer: %s\n", ENABLE_STATS_TIMER ? "ENABLED" : "DISABLED");
    printk("Expected CPU wake-ups per second:\n");
    printk("  - ADC: ~%d Hz (batched)\n", 100 / ADC_INTERRUPT_BATCH_SIZE);
    printk("  - State: ~%d Hz\n", ble_get_frequency_hz() * 2);
    printk("  - Stats: %d Hz\n", ENABLE_STATS_TIMER ? 1 : 0);
    printk("  - BLE: variable\n");
    printk("================================\n\n");

    while (1) {
        k_sleep(K_FOREVER);
    }
}