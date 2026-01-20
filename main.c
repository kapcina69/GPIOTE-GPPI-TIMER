/*
 * Multi-timer verzija sa GPPI - FULL DEBUG
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

// Timer instance indices
#define TIMER_PULSE_IDX 1      // Za generisanje pulseva (GPPI kontrolisan)
#define TIMER_STATE_IDX 2      // Za state machine kontrolu
#define GPIOTE_INST_IDX 0

// GPIO pins
#define OUTPUT_PIN_1 LED1_PIN
#define OUTPUT_PIN_2 LED2_PIN

// SAADC config
#define SAADC_CHANNEL_AIN NRF_SAADC_INPUT_AIN0
#define SAADC_RESOLUTION NRF_SAADC_RESOLUTION_10BIT

#define LOG_EVERY_N_SAMPLES 1  // Ispisuj napon za svaki sample

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

// ADC variables - Double buffer for continuous sampling
static int16_t m_saadc_buffer[2];  // Two buffers for ping-pong
static uint8_t m_saadc_completed_buffer_idx = 0;  // Which buffer just completed (for recycling)
static uint32_t sample_counter = 0;
static int16_t m_saadc_sample;  // Last read sample value

// Zephyr timer za periodični ispis statistike
static void stats_timer_callback(struct k_timer *timer);
K_TIMER_DEFINE(stats_timer, stats_timer_callback, NULL);
static uint32_t last_sample_count = 0;

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
    STATE_ACTIVE,    // Pulsevi se generišu
    STATE_PAUSE      // Pauza između ciklusa
} state_t;

static volatile state_t current_state = STATE_ACTIVE;
static volatile uint32_t state_transitions = 0;

/**
 * @brief Convert ADC sample to millivolts
 */
static int32_t saadc_sample_to_mv(int16_t sample)
{
    return ((int32_t)sample * 3600) / 1024;
}

/**
 * @brief SAADC event handler
 */
static void saadc_handler(nrfx_saadc_evt_t const * p_event)
{
    nrfx_err_t status;
    
    switch (p_event->type)
    {
        case NRFX_SAADC_EVT_BUF_REQ:
            // KRITIČNO: Postavi ISTI buffer koji je upravo završen da bi se mogao koristiti ponovo
            // Ovo omogućava kontinualno double-buffering
            {
                status = nrfx_saadc_buffer_set(&m_saadc_buffer[m_saadc_completed_buffer_idx], 1);
                // No logging - happens too frequently
            }
            break;

        case NRFX_SAADC_EVT_DONE:
        {
            // Izvuci sample iz završenog buffer-a
            m_saadc_sample = NRFX_SAADC_SAMPLE_GET(SAADC_RESOLUTION, 
                                                   p_event->data.done.p_buffer, 
                                                   0);
            
            // Prati koji je buffer završen za BUF_REQ
            m_saadc_completed_buffer_idx = 1 - m_saadc_completed_buffer_idx;
            
            sample_counter++;
            
            // Note: With start_on_end=true, SAADC automatically re-arms after DONE
            // No manual mode_trigger() call needed!
            
            if (sample_counter % LOG_EVERY_N_SAMPLES == 0)
            {
                int32_t voltage_mv = saadc_sample_to_mv(m_saadc_sample);
                uint32_t capture_val = nrfx_timer_capture_get(&timer_pulse, 
                                                               NRF_TIMER_CC_CHANNEL4);
                
                // printk("[ADC #%u] V=%d mV, Capture=%u ticks\n",
                //        sample_counter, voltage_mv, capture_val);
            }
            break;
        }

        case NRFX_SAADC_EVT_READY:
            printk("[ADC_READY]\n");
            break;

        case NRFX_SAADC_EVT_CALIBRATEDONE:
            printk("[ADC_CAL_DONE]\n");
            break;

        case NRFX_SAADC_EVT_FINISHED:
            // Restart SAADC odmah u interrupt handleru (bez kašnjenja main loop-a)
            nrfx_saadc_buffer_set(&m_saadc_buffer[0], 1);
            m_saadc_completed_buffer_idx = 1;
            nrfx_saadc_mode_trigger();
            // No logging - happens too frequently
            break;

        default:
            printk("[ADC_EVENT] Unknown: %d\n", p_event->type);
            NRFX_LOG_WARNING("SAADC: Unknown event type: %d", p_event->type);
            break;
    }
}

/**
 * @brief Periodični ispis statistike (poziva se svakih 1s)
 */
static void stats_timer_callback(struct k_timer *timer)
{
    uint32_t samples_since_last = sample_counter - last_sample_count;
    printk("[STATS] State: %s, Samples: %u (+%u), Transitions: %u\n",
           current_state == STATE_ACTIVE ? "ACTIVE" : "PAUSE",
           sample_counter,
           samples_since_last,
           state_transitions);
    last_sample_count = sample_counter;
}

/**
 * @brief Setup pulse generation timer sa GPPI
 */
static void setup_pulse_timer(uint32_t pulse_us)
{
    NRFX_LOG_INFO("=== PULSE TIMER SETUP START ===");
    
    nrfx_timer_disable(&timer_pulse);
    NRFX_LOG_DEBUG("Pulse timer disabled");
    
    nrfx_timer_clear(&timer_pulse);
    NRFX_LOG_DEBUG("Pulse timer cleared");
    
    uint32_t pulse_ticks = nrfx_timer_us_to_ticks(&timer_pulse, pulse_us);
    
    NRFX_LOG_INFO("Pulse setup: %u us = %u ticks", pulse_us, pulse_ticks);
    
    // CC0: Početak ciklusa - digni PIN1
    nrfx_timer_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL0, 10, false);
    NRFX_LOG_DEBUG("CC0 = 10 ticks (start delay)");
    
    // CC1: Spusti PIN1 (posle pulse_us)
    nrfx_timer_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL1, pulse_ticks + 10, false);
    NRFX_LOG_DEBUG("CC1 = %u ticks (PIN1 end)", pulse_ticks + 10);
    
    // CC2: Digni PIN2 (odmah posle PIN1)
    nrfx_timer_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL2, 
                       pulse_ticks + 20, false);
    NRFX_LOG_DEBUG("CC2 = %u ticks (PIN2 start)", pulse_ticks + 20);
    
    // CC3: Spusti PIN2 (posle još pulse_us)
    nrfx_timer_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL3, 
                       pulse_ticks * 2 + 20, false);
    NRFX_LOG_DEBUG("CC3 = %u ticks (PIN2 end)", pulse_ticks * 2 + 20);
    
    // CC4: Za ADC timing capture (samo capture, ne koristi se u compare)
    NRFX_LOG_DEBUG("CC4 = CAPTURE only (ADC timing)");
    
    // CC5: Kraj ciklusa - reset timer
    nrfx_timer_extended_compare(&timer_pulse, NRF_TIMER_CC_CHANNEL5,
                                pulse_ticks * 2 + 30,
                                NRF_TIMER_SHORT_COMPARE5_CLEAR_MASK, false);
    NRFX_LOG_DEBUG("CC5 = %u ticks (timer reset)", pulse_ticks * 2 + 30);
    
    NRFX_LOG_INFO("Timer compare values: CC0=10, CC1=%u, CC2=%u, CC3=%u, CC5=%u",
                  pulse_ticks + 10, pulse_ticks + 20, pulse_ticks * 2 + 20, pulse_ticks * 2 + 30);
    
    nrfx_timer_enable(&timer_pulse);
    NRFX_LOG_INFO("Pulse timer ENABLED");
    NRFX_LOG_INFO("=== PULSE TIMER SETUP END ===");
}

/**
 * @brief Setup GPPI connections
 */
static void setup_gppi_connections(void)
{
    NRFX_LOG_INFO("=== GPPI SETUP START ===");
    
    // Dobij task adrese
    uint32_t pin1_set_addr = (uint32_t)&NRF_GPIOTE->TASKS_SET[gpiote_ch_pin1];
    uint32_t pin1_clr_addr = (uint32_t)&NRF_GPIOTE->TASKS_CLR[gpiote_ch_pin1];
    uint32_t pin2_set_addr = (uint32_t)&NRF_GPIOTE->TASKS_SET[gpiote_ch_pin2];
    uint32_t pin2_clr_addr = (uint32_t)&NRF_GPIOTE->TASKS_CLR[gpiote_ch_pin2];
    
    NRFX_LOG_INFO("GPIOTE channels: PIN1=%u, PIN2=%u", gpiote_ch_pin1, gpiote_ch_pin2);
    NRFX_LOG_INFO("PIN1 tasks: SET=0x%08X, CLR=0x%08X", pin1_set_addr, pin1_clr_addr);
    NRFX_LOG_INFO("PIN2 tasks: SET=0x%08X, CLR=0x%08X", pin2_set_addr, pin2_clr_addr);
    
    // Dobij event adrese
    uint32_t timer_cc0_event = nrfx_timer_compare_event_address_get(&timer_pulse, NRF_TIMER_CC_CHANNEL0);
    uint32_t timer_cc1_event = nrfx_timer_compare_event_address_get(&timer_pulse, NRF_TIMER_CC_CHANNEL1);
    uint32_t timer_cc2_event = nrfx_timer_compare_event_address_get(&timer_pulse, NRF_TIMER_CC_CHANNEL2);
    uint32_t timer_cc3_event = nrfx_timer_compare_event_address_get(&timer_pulse, NRF_TIMER_CC_CHANNEL3);
    
    NRFX_LOG_INFO("Timer events: CC0=0x%08X, CC1=0x%08X, CC2=0x%08X, CC3=0x%08X",
                  timer_cc0_event, timer_cc1_event, timer_cc2_event, timer_cc3_event);
    
    // ADC trigger koristi TIMER1 CC1 (kada PIN1 ide HIGH)
    NRFX_LOG_INFO("ADC trigger: TIMER1 CC1 event (0x%08X)", timer_cc1_event);
    
    uint32_t saadc_sample_task = nrf_saadc_task_address_get(NRF_SAADC, NRF_SAADC_TASK_SAMPLE);
    uint32_t saadc_end_event = nrf_saadc_event_address_get(NRF_SAADC, NRF_SAADC_EVENT_END);
    uint32_t timer_capture_task = nrfx_timer_task_address_get(&timer_pulse, NRF_TIMER_TASK_CAPTURE4);
    
    NRFX_LOG_INFO("SAADC: SAMPLE task=0x%08X, END event=0x%08X", saadc_sample_task, saadc_end_event);
    NRFX_LOG_INFO("Timer CAPTURE4 task=0x%08X", timer_capture_task);
    
    NRFX_LOG_INFO("GPPI channels: pin1_set=%u, pin1_clr=%u, pin2_set=%u, pin2_clr=%u, adc_trig=%u, adc_cap=%u",
                  gppi_pin1_set, gppi_pin1_clr, gppi_pin2_set, gppi_pin2_clr, gppi_adc_trigger, gppi_adc_capture);
    
    // CC0: PIN1 CLR (aktiviraj - LOW)
    NRFX_LOG_DEBUG("Setting up GPPI[%u]: Timer CC0 -> PIN1 CLR", gppi_pin1_set);
    nrfx_gppi_channel_endpoints_setup(gppi_pin1_set,
        timer_cc0_event,
        pin1_clr_addr);
    
    // TIMER1 CC1 -> SAADC SAMPLE (trigger kada PIN1 ide na HIGH - kraj pulsa)
    NRFX_LOG_DEBUG("Setting up GPPI[%u]: TIMER1 CC1 -> SAADC SAMPLE", gppi_adc_trigger);
    nrfx_gppi_channel_endpoints_setup(gppi_adc_trigger,
        timer_cc1_event,  // Koristi CC1 umesto ADC timer-a
        saadc_sample_task);
    
    // CC1: PIN1 SET (deaktiviraj - HIGH)
    NRFX_LOG_DEBUG("Setting up GPPI[%u]: Timer CC1 -> PIN1 SET", gppi_pin1_clr);
    nrfx_gppi_channel_endpoints_setup(gppi_pin1_clr,
        timer_cc1_event,
        pin1_set_addr);
    
    // CC2: PIN2 CLR (aktiviraj - LOW)
    NRFX_LOG_DEBUG("Setting up GPPI[%u]: Timer CC2 -> PIN2 CLR", gppi_pin2_set);
    nrfx_gppi_channel_endpoints_setup(gppi_pin2_set,
        timer_cc2_event,
        pin2_clr_addr);
    
    // CC3: PIN2 SET (deaktiviraj - HIGH)
    NRFX_LOG_DEBUG("Setting up GPPI[%u]: Timer CC3 -> PIN2 SET", gppi_pin2_clr);
    nrfx_gppi_channel_endpoints_setup(gppi_pin2_clr,
        timer_cc3_event,
        pin2_set_addr);
    
    // SAADC END → Timer CAPTURE4
    NRFX_LOG_DEBUG("Setting up GPPI[%u]: SAADC END -> Timer CAPTURE4", gppi_adc_capture);
    nrfx_gppi_channel_endpoints_setup(gppi_adc_capture,
        saadc_end_event,
        timer_capture_task);
    
    // Enable sve kanale
    uint32_t channels_mask = BIT(gppi_pin1_set) | BIT(gppi_pin1_clr) |
                             BIT(gppi_pin2_set) | BIT(gppi_pin2_clr) |
                             BIT(gppi_adc_trigger) | BIT(gppi_adc_capture);
    
    NRFX_LOG_INFO("Enabling GPPI channels mask: 0x%08X", channels_mask);
    nrfx_gppi_channels_enable(channels_mask);
    
    printk("GPPI channels enabled (mask: 0x%02X, ADC trigger ch=%u)\\n", channels_mask, gppi_adc_trigger);
    
    // ADC Timer (TIMER0) is no longer needed - using TIMER1 CC1 for ADC trigger
    printk("[ADC_TRIGGER] Using TIMER1 CC1 event for SAADC sampling\n");
    
    NRFX_LOG_INFO("=== GPPI SETUP END ===");
}

/**
 * @brief State machine timer handler
 */
static void state_timer_handler(nrf_timer_event_t event_type, void * p_context)
{
    if (event_type == NRF_TIMER_EVENT_COMPARE0)
    {
        state_transitions++;
        
        // KRITIČNO: printk odmah ispisuje, ne čeka log proces
        // printk("*** STATE TIMER FIRED! Transition #%u, State: %s ***\n",
        //        state_transitions,
        //        current_state == STATE_ACTIVE ? "ACTIVE" : "PAUSE");
        
        // NRFX_LOG_DEBUG("State timer event, transition #%u, current state: %s",
        //               state_transitions,
        //               current_state == STATE_ACTIVE ? "ACTIVE" : "PAUSE");
        
        if (ble_parameters_updated())
        {
            ble_clear_update_flag();
            
            uint32_t new_pulse_us = ble_get_pulse_width_ms() * 100;
            uint32_t new_pause_ms = ble_get_pause_time_ms();
            
            // printk("*** BLE UPDATE: pulse=%u us, pause=%u ms ***\n", new_pulse_us, new_pause_ms);
            
            NRFX_LOG_INFO("BLE parameters updated: pulse_us=%u, pause_ms=%u",
                         new_pulse_us, new_pause_ms);
            
            // Rekonfiguriši pulse timer (disable+configure)
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
            
            // Enable SAMO ako smo trenutno u ACTIVE stanju
            if (current_state == STATE_ACTIVE) {
                nrfx_timer_enable(&timer_pulse);
                printk("[BLE] Pulse timer reconfigured and enabled (ACTIVE state)\n");
            } else {
                printk("[BLE] Pulse timer reconfigured but NOT enabled (PAUSE state)\n");
            }
        }
        
        switch(current_state)
        {
            case STATE_ACTIVE:
                NRFX_LOG_DEBUG("State: ACTIVE -> PAUSE");
                
                // Zaustavi pulse timer
                nrfx_timer_disable(&timer_pulse);
                NRFX_LOG_DEBUG("Pulse timer disabled");
                
                current_state = STATE_PAUSE;
                
                // Setup pauzu - izračunaj precizno da ukupan period odgovara frekvenciji
                // Total period = 1000000/freq_hz (µs), PAUSE = Total - ACTIVE
                uint32_t freq_hz = ble_get_frequency_hz();
                uint32_t pulse_us_pause = ble_get_pulse_width_ms() * 100;
                uint32_t active_period_us = pulse_us_pause * 2 + 100;
                
                // Izračunaj total period iz frekvencije u µs
                uint32_t total_period_us = 1000000 / freq_hz;
                
                // PAUSE = total - active (u µs)
                uint32_t pause_us;
                if (total_period_us > active_period_us) {
                    pause_us = total_period_us - active_period_us;
                } else {
                    pause_us = 0;  // Bez pauze ako je active >= total
                }
                
                NRFX_LOG_DEBUG("Setting up PAUSE for %u us (freq=%u Hz, total=%u us, active=%u us)",
                              pause_us, freq_hz, total_period_us, active_period_us);
                
                // Resetuj timer na 0 da PAUSE traje tačno pause_us
                nrfx_timer_disable(&timer_state);
                nrfx_timer_clear(&timer_state);
                uint32_t pause_ticks = nrfx_timer_us_to_ticks(&timer_state, pause_us);
                nrfx_timer_compare(&timer_state, NRF_TIMER_CC_CHANNEL0, 
                                  pause_ticks, true);
                nrfx_timer_enable(&timer_state);
                
                NRFX_LOG_DEBUG("Pause timer set to %u ticks", pause_ticks);
                break;
                
            case STATE_PAUSE:
                NRFX_LOG_DEBUG("State: PAUSE -> ACTIVE");
                
                // Samo enable pulse timer - već je konfigurisan, samo treba da krene
                nrfx_timer_enable(&timer_pulse);
                
                current_state = STATE_ACTIVE;
                
                // Setup vreme trajanja aktivnog perioda
                uint32_t pulse_us = ble_get_pulse_width_ms() * 100;
                uint32_t active_us = pulse_us * 2 + 100;
                
                NRFX_LOG_DEBUG("Setting up ACTIVE for %u us", active_us);
                
                // Resetuj timer na 0 da ACTIVE traje tačno active_us
                nrfx_timer_disable(&timer_state);
                nrfx_timer_clear(&timer_state);
                uint32_t active_ticks = nrfx_timer_us_to_ticks(&timer_state, active_us);
                nrfx_timer_compare(&timer_state, NRF_TIMER_CC_CHANNEL0,
                                  active_ticks, true);
                nrfx_timer_enable(&timer_state);
                
                NRFX_LOG_DEBUG("Active timer set to %u ticks", active_ticks);
                break;
        }
    }
}

int main(void)
{
    nrfx_err_t status;
    (void)status;

#if defined(__ZEPHYR__)
    IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_TIMER_INST_GET(TIMER_STATE_IDX)), IRQ_PRIO_LOWEST,
                NRFX_TIMER_INST_HANDLER_GET(TIMER_STATE_IDX), 0, 0);
    IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_GPIOTE_INST_GET(GPIOTE_INST_IDX)), IRQ_PRIO_LOWEST,
                NRFX_GPIOTE_INST_HANDLER_GET(GPIOTE_INST_IDX), 0, 0);
    IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_SAADC), IRQ_PRIO_LOWEST, 
                nrfx_saadc_irq_handler, 0, 0);
#endif

    NRFX_EXAMPLE_LOG_INIT();
    NRFX_LOG_INFO("========================================");
    NRFX_LOG_INFO("Multi-timer GPPI pulse generator - DEBUG");
    NRFX_LOG_INFO("========================================");
    NRFX_EXAMPLE_LOG_PROCESS();

    // ========== SAADC INIT ==========
    NRFX_LOG_INFO("=== SAADC INITIALIZATION ===");
    
    status = nrfx_saadc_init(NRFX_SAADC_DEFAULT_CONFIG_IRQ_PRIORITY);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    NRFX_LOG_DEBUG("SAADC driver initialized");

    status = nrfx_saadc_channel_config(&m_saadc_channel);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    NRFX_LOG_DEBUG("SAADC channel configured: AIN0");

    uint32_t channels_mask = nrfx_saadc_channels_configured_get();
    NRFX_LOG_DEBUG("SAADC channels mask: 0x%08X", channels_mask);
    
    status = nrfx_saadc_advanced_mode_set(channels_mask,
                                           SAADC_RESOLUTION,
                                           &(nrfx_saadc_adv_config_t){
                                               .oversampling = NRF_SAADC_OVERSAMPLE_DISABLED,
                                               .burst = NRF_SAADC_BURST_DISABLED,
                                               .internal_timer_cc = 0,
                                               .start_on_end = true,  // Auto re-arm after DONE for continuous triggering
                                           },
                                           saadc_handler);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    NRFX_LOG_DEBUG("SAADC advanced mode set");

    status = nrfx_saadc_buffer_set(&m_saadc_buffer[0], 1);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    NRFX_LOG_DEBUG("SAADC buffer set");
    printk("[SAADC_INIT] Buffer set: ptr=0x%p (buffer[0]), size=1\n", &m_saadc_buffer[0]);
    m_saadc_completed_buffer_idx = 1;  // First completed will be buffer 1
    
    // ENABLE SAADC explicitly (advanced mode doesn't auto-enable)
    nrf_saadc_enable(NRF_SAADC);
    printk("[SAADC_INIT] SAADC enabled explicitly\n");
    
    // ARM SAADC for external triggers - this prepares the SAADC to respond to GPPI SAMPLE tasks
    // Must be called BEFORE pulse timer starts to prevent premature triggers
    status = nrfx_saadc_mode_trigger();
    NRFX_ASSERT(status == NRFX_SUCCESS);
    printk("[SAADC_INIT] Advanced mode ARMED via mode_trigger() - ready for GPPI triggers\n");
    
    // Detailed register dump
    printk("\n=== SAADC REGISTER DUMP (AFTER INIT) ===\n");
    printk("  ENABLE:        0x%08X (should be 1)\n", NRF_SAADC->ENABLE);
    printk("  STATUS:        0x%08X\n", NRF_SAADC->STATUS);
    printk("  RESOLUTION:    0x%08X\n", NRF_SAADC->RESOLUTION);
    printk("  RESULT.PTR:    0x%08X (should be 0x%08X)\n", NRF_SAADC->RESULT.PTR, (uint32_t)&m_saadc_sample);
    printk("  RESULT.MAXCNT: 0x%08X\n", NRF_SAADC->RESULT.MAXCNT);
    printk("  RESULT.AMOUNT: 0x%08X\n", NRF_SAADC->RESULT.AMOUNT);
    printk("  INTEN:         0x%08X\n", NRF_SAADC->INTEN);
    printk("  INTENSET:      0x%08X\n", NRF_SAADC->INTENSET);
    printk("  CH[0].PSELP:   0x%08X\n", NRF_SAADC->CH[0].PSELP);
    printk("  CH[0].PSELN:   0x%08X\n", NRF_SAADC->CH[0].PSELN);
    printk("  CH[0].CONFIG:  0x%08X\n", NRF_SAADC->CH[0].CONFIG);
    printk("=== END REGISTER DUMP ===\n\n");
    
    NRFX_LOG_INFO("SAADC initialization complete");
    printk("[MARK] SAADC INIT DONE - Proceeding to GPIOTE\n\n");
    printk("  CH[0].CONFIG:  0x%08X\n", NRF_SAADC->CH[0].CONFIG);
    printk("  CH[0].PSELN:   0x%08X\n", NRF_SAADC->CH[0].PSELN);
    printk("=== END REGISTER DUMP ===\n\n");
    
    printk("SAADC status after init: ENABLE=0x%08X, STATUS=0x%08X\n",
           NRF_SAADC->ENABLE, NRF_SAADC->STATUS);
    
    NRFX_LOG_INFO("SAADC initialization complete");

    // ========== GPIOTE INIT ==========
    NRFX_LOG_INFO("=== GPIOTE INITIALIZATION ===");
    
    nrfx_gpiote_t const gpiote_inst = NRFX_GPIOTE_INSTANCE(GPIOTE_INST_IDX);
    p_gpiote_inst = &gpiote_inst;
    
    status = nrfx_gpiote_init(&gpiote_inst, NRFX_GPIOTE_DEFAULT_CONFIG_IRQ_PRIORITY);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    NRFX_LOG_DEBUG("GPIOTE driver initialized");

    // Alociraj GPIOTE kanale
    status = nrfx_gpiote_channel_alloc(&gpiote_inst, &gpiote_ch_pin1);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    NRFX_LOG_DEBUG("GPIOTE channel allocated for PIN1: channel %u", gpiote_ch_pin1);
    
    status = nrfx_gpiote_channel_alloc(&gpiote_inst, &gpiote_ch_pin2);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    NRFX_LOG_DEBUG("GPIOTE channel allocated for PIN2: channel %u", gpiote_ch_pin2);

    // Konfiguriši OUTPUT pinove sa task-ovima
    static const nrfx_gpiote_output_config_t output_config = {
        .drive = NRF_GPIO_PIN_S0S1,
        .input_connect = NRF_GPIO_PIN_INPUT_DISCONNECT,
        .pull = NRF_GPIO_PIN_NOPULL,
    };

    // PIN1: inicijalno HIGH (neaktivan), aktivan je LOW
    const nrfx_gpiote_task_config_t task_config_pin1 = {
        .task_ch = gpiote_ch_pin1,
        .polarity = NRF_GPIOTE_POLARITY_LOTOHI,
        .init_val = NRF_GPIOTE_INITIAL_VALUE_HIGH,
    };

    NRFX_LOG_DEBUG("Configuring PIN1 (pin %u) with channel %u", OUTPUT_PIN_1, gpiote_ch_pin1);
    status = nrfx_gpiote_output_configure(&gpiote_inst, OUTPUT_PIN_1,
                                          &output_config, &task_config_pin1);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    
    nrfx_gpiote_out_task_enable(&gpiote_inst, OUTPUT_PIN_1);
    NRFX_LOG_INFO("PIN1 configured: pin=%u, channel=%u, init=HIGH", 
                  OUTPUT_PIN_1, gpiote_ch_pin1);

    // PIN2: inicijalno HIGH (neaktivan), aktivan je LOW
    const nrfx_gpiote_task_config_t task_config_pin2 = {
        .task_ch = gpiote_ch_pin2,
        .polarity = NRF_GPIOTE_POLARITY_LOTOHI,
        .init_val = NRF_GPIOTE_INITIAL_VALUE_HIGH,
    };

    NRFX_LOG_DEBUG("Configuring PIN2 (pin %u) with channel %u", OUTPUT_PIN_2, gpiote_ch_pin2);
    status = nrfx_gpiote_output_configure(&gpiote_inst, OUTPUT_PIN_2,
                                          &output_config, &task_config_pin2);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    
    nrfx_gpiote_out_task_enable(&gpiote_inst, OUTPUT_PIN_2);
    NRFX_LOG_INFO("PIN2 configured: pin=%u, channel=%u, init=HIGH", 
                  OUTPUT_PIN_2, gpiote_ch_pin2);

    // ========== GPPI CHANNELS ALLOC ==========
    NRFX_LOG_INFO("=== GPPI CHANNEL ALLOCATION ===");
    
    status = nrfx_gppi_channel_alloc(&gppi_pin1_set);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    NRFX_LOG_DEBUG("GPPI channel allocated: gppi_pin1_set = %u", gppi_pin1_set);
    
    status = nrfx_gppi_channel_alloc(&gppi_pin1_clr);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    NRFX_LOG_DEBUG("GPPI channel allocated: gppi_pin1_clr = %u", gppi_pin1_clr);
    
    status = nrfx_gppi_channel_alloc(&gppi_pin2_set);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    NRFX_LOG_DEBUG("GPPI channel allocated: gppi_pin2_set = %u", gppi_pin2_set);
    
    status = nrfx_gppi_channel_alloc(&gppi_pin2_clr);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    NRFX_LOG_DEBUG("GPPI channel allocated: gppi_pin2_clr = %u", gppi_pin2_clr);
    
    status = nrfx_gppi_channel_alloc(&gppi_adc_trigger);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    NRFX_LOG_DEBUG("GPPI channel allocated: gppi_adc_trigger = %u", gppi_adc_trigger);
    
    status = nrfx_gppi_channel_alloc(&gppi_adc_capture);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    NRFX_LOG_DEBUG("GPPI channel allocated: gppi_adc_capture = %u", gppi_adc_capture);
    
    NRFX_LOG_INFO("All GPPI channels allocated successfully");

    // ========== PULSE TIMER INIT ==========
    NRFX_LOG_INFO("=== PULSE TIMER INITIALIZATION ===");
    
    uint32_t base_freq_pulse = NRF_TIMER_BASE_FREQUENCY_GET(timer_pulse.p_reg);
    NRFX_LOG_DEBUG("Pulse timer base frequency: %u Hz", base_freq_pulse);
    
    nrfx_timer_config_t pulse_config = NRFX_TIMER_DEFAULT_CONFIG(base_freq_pulse);
    pulse_config.bit_width = NRF_TIMER_BIT_WIDTH_32;
    pulse_config.p_context = (void*)"pulse_timer";

    status = nrfx_timer_init(&timer_pulse, &pulse_config, NULL);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    NRFX_LOG_INFO("Pulse timer initialized: freq=%u Hz, width=32bit", base_freq_pulse);

    // ========== STATE TIMER INIT ==========
    NRFX_LOG_INFO("=== STATE TIMER INITIALIZATION ===");
    
    uint32_t base_freq_state = NRF_TIMER_BASE_FREQUENCY_GET(timer_state.p_reg);
    NRFX_LOG_DEBUG("State timer base frequency: %u Hz", base_freq_state);
    
    nrfx_timer_config_t state_config = NRFX_TIMER_DEFAULT_CONFIG(base_freq_state);
    state_config.bit_width = NRF_TIMER_BIT_WIDTH_32;
    state_config.p_context = (void*)"state_timer";

    status = nrfx_timer_init(&timer_state, &state_config, state_timer_handler);
    NRFX_ASSERT(status == NRFX_SUCCESS);
    NRFX_LOG_INFO("State timer initialized: freq=%u Hz, width=32bit", base_freq_state);

    // ========== SETUP ==========
    NRFX_LOG_INFO("=== MAIN SETUP ===");
    
    uint32_t pulse_us = ble_get_pulse_width_ms() * 100;
    NRFX_LOG_INFO("Initial pulse width: %u ms -> %u us", ble_get_pulse_width_ms(), pulse_us);
    NRFX_LOG_INFO("Initial pause time: %u ms", ble_get_pause_time_ms());
    
    setup_pulse_timer(pulse_us);
    setup_gppi_connections();

    // Pokreni state timer
    current_state = STATE_ACTIVE;
    uint32_t active_us = pulse_us * 2 + 100;
    uint32_t active_ticks = nrfx_timer_us_to_ticks(&timer_state, active_us);
    
    NRFX_LOG_INFO("Starting state timer: ACTIVE mode, duration=%u us (%u ticks)",
                  active_us, active_ticks);
    
    nrfx_timer_compare(&timer_state, NRF_TIMER_CC_CHANNEL0, active_ticks, true);
    nrfx_timer_enable(&timer_state);
    
    NRFX_LOG_INFO("State timer ENABLED");
    NRFX_LOG_INFO("=== INITIALIZATION COMPLETE ===");
    NRFX_LOG_INFO("Started: pulse=%u us, pause=%u ms", pulse_us, ble_get_pause_time_ms());

    // ========== BLE INIT ==========
    NRFX_LOG_INFO("=== BLE INITIALIZATION ===");
    printk("\n[MARK] Starting BLE init...\n");
    int ble_err = ble_init();
    if (!ble_err) {
        NRFX_LOG_INFO("BLE initialized and ready");
    } else {
        NRFX_LOG_ERROR("BLE initialization failed: %d", ble_err);
    }

    NRFX_LOG_INFO("========================================");
    NRFX_LOG_INFO("Entering main loop");
    NRFX_LOG_INFO("========================================");

    // MANUAL TEST: Da li GPIOTE task-ovi rade?
    printk("\n=== MANUAL GPIO TEST START ===\n");
    
    printk("Testing PIN1 manual control...\n");
    // Test SET task (HIGH)
    NRF_GPIOTE->TASKS_SET[gpiote_ch_pin1] = 1;
    k_sleep(K_MSEC(500));
    
    // Test CLR task (LOW)
    NRF_GPIOTE->TASKS_CLR[gpiote_ch_pin1] = 1;
    k_sleep(K_MSEC(500));
    
    // Test SET again
    NRF_GPIOTE->TASKS_SET[gpiote_ch_pin1] = 1;
    k_sleep(K_MSEC(500));
    
    printk("PIN1 manual test done - check logic analyzer!\n");
    
    printk("Testing PIN2 manual control...\n");
    // Test PIN2
    NRF_GPIOTE->TASKS_CLR[gpiote_ch_pin2] = 1;
    k_sleep(K_MSEC(500));
    NRF_GPIOTE->TASKS_SET[gpiote_ch_pin2] = 1;
    k_sleep(K_MSEC(500));
    
    printk("PIN2 manual test done\n");
    printk("=== MANUAL GPIO TEST END ===\n\n");
    
    // Test SAADC manual trigger
    printk("=== MANUAL SAADC TEST START ===\n");
    sample_counter = 0;
    
    printk("SAADC state before manual trigger:\n");
    printk("  ENABLE=%u, STATUS=0x%02X, RESULT.AMOUNT=%u\n",
           NRF_SAADC->ENABLE, NRF_SAADC->STATUS, NRF_SAADC->RESULT.AMOUNT);
    
    printk("Triggering SAADC manually...\n");
    nrf_saadc_enable(NRF_SAADC);
    nrfx_saadc_mode_trigger();
    NRF_SAADC->TASKS_SAMPLE = 1;
    
    k_sleep(K_MSEC(50));
    
    printk("SAADC state after trigger (50ms wait):\n");
    printk("  ENABLE=%u, STATUS=0x%02X, RESULT.AMOUNT=%u, Samples: %u\n",
           NRF_SAADC->ENABLE, NRF_SAADC->STATUS, NRF_SAADC->RESULT.AMOUNT, sample_counter);
    
    printk("=== MANUAL SAADC TEST END ===\n\n");
    
    // Reset counter
    sample_counter = 0;

    // Pokreni periodični timer za ispis statistike (svakih 1s)
    k_timer_start(&stats_timer, K_SECONDS(1), K_SECONDS(1));
    printk("[MAIN] Statistics timer started (1s period)\n");
    
    printk("[MAIN] Entering event-driven mode - all work done in interrupts\n");
    printk("[MAIN] Main thread sleeping, CPU available for BLE and other tasks\n\n");

    // Event-driven dizajn: sve radi u interrupt handler-ima
    // - LED pulsevi: TIMER1 → GPPI → GPIOTE (hardware)
    // - ADC trigger: TIMER1 CC1 → GPPI → SAADC (hardware)
    // - ADC restart: FINISHED handler (interrupt)
    // - State machine: TIMER2 handler (interrupt)
    // - Statistika: Zephyr timer callback (software timer)
    while (1)
    {
        // Main thread spava - sve se dešava kroz interrupt-e i hardware
        // Ovo oslobađa CPU za BLE stack i ostale zadatke
        k_sleep(K_FOREVER);
    }
}