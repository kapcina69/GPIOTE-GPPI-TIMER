#include "uart.h"
#include <nrfx_uarte.h>
#include <zephyr/irq.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include "../config.h"
#include "../drivers/dac/dac.h"
#include "../drivers/timers/timer.h"

LOG_MODULE_REGISTER(uart_cmd, LOG_LEVEL_DBG);

/* ============================================================
 *                    CONSTANTS & DEFINES
 * ============================================================ */

// Timeout konstante
#define TX_BUSY_TIMEOUT_ITERATIONS  1000
#define TX_BUSY_WAIT_US             100

// Broj elemenata u nizu
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* ============================================================
 *                    TYPE DEFINITIONS
 * ============================================================ */

// Tip za command handler funkciju
typedef void (*cmd_handler_t)(const char *args);

// Struktura za jednu komandu u lookup tabeli
typedef struct {
    const char *prefix;      // Prefiks komande (npr. "SON", "PW;")
    size_t prefix_len;       // Dužina prefiksa
    bool has_args;           // Da li ima argumente nakon prefiksa
    cmd_handler_t handler;   // Function pointer na handler
} cmd_entry_t;

/* ============================================================
 *                    PRIVATE VARIABLES
 * ============================================================ */

// Privatni baferi
static uint8_t m_rx_chunk[RX_CHUNK_SIZE];
static char cmd_buffer[CMD_BUFFER_SIZE];
static char tx_buffer[TX_BUFFER_SIZE];
static size_t cmd_index = 0;
static bool cmd_started = false;
static volatile bool tx_busy = false;
static nrfx_uarte_t uarte_inst = NRFX_UARTE_INSTANCE(UARTE_INST_IDX);

volatile uint32_t current_frequency_hz = 1;
volatile uint32_t current_pulse_width = 5;
volatile bool parameters_updated = false;

// Sockeet parametri
static bool stimulation_running = false;
static float voltage_amplitude = 1.0f;
static uint16_t cathode_channels[16] = {0};

// Test komande za periodično slanje
static const char* test_commands[] = {
    ">SON<",
    ">SA;0000 0200 0400 0600 0800 0A00<",
    ">SC;0100 0004 4000 0020 0001 0800 0008 2000 0002 1000 0040 8000 0010 0200 0008 0400<",
    ">SC;0001 0002<",
    ">PW;a<"
    // ">SOFF<",
    // ">PW;1<",
    // ">SON<",
    // ">SA;0064<",
    // ">SF;19<",
    // ">SON<",
    // ">PW;1<",
    // ">PW;3<",
    // ">PW;7<",
    // ">PW;10<",
    // ">SA;32<",
    // ">SA;C8<",
    // ">SA;2C<",
    // ">SF;A<",
    // ">SF;32<",
    // ">SF;50<",
    // ">SON<",
    // ">SOFF<",
    // ">PW;8<",
    // ">SA;FA<",
    // ">SF;64<"
};
static uint8_t current_cmd_index = 0;

/* ============================================================
 *                    FORWARD DECLARATIONS
 * ============================================================ */

// UART driver
static void uarte_handler(nrfx_uarte_event_t const *p_event, void *p_context);
static void process_command(const char *cmd);

// Command handlers
static void handle_son(const char *args);
static void handle_soff(const char *args);
static void handle_pw(const char *args);
static void handle_sa(const char *args);
static void handle_sf(const char *args);
static void handle_sc(const char *args);

// Helper functions
static void print_current_state(void);

/* ============================================================
 *                    COMMAND LOOKUP TABLE
 * ============================================================ */

static const cmd_entry_t cmd_table[] = {
    {"SON",  3, false, handle_son},
    {"SOFF", 4, false, handle_soff},
    {"PW;",  3, true,  handle_pw},
    {"SA;",  3, true,  handle_sa},
    {"SF;",  3, true,  handle_sf},
    {"SC;",  3, true,  handle_sc},
};

/**
 * @brief Šalje odgovor nazad kroz UART
 */
void uart_send_response(const char *response)
{
    nrfx_err_t status;
    
    uint32_t timeout = TX_BUSY_TIMEOUT_ITERATIONS;
    while (tx_busy && timeout > 0) {
        k_busy_wait(TX_BUSY_WAIT_US);
        timeout--;
    }
    
    if (tx_busy) {
        printk("WARNING: TX still busy, response dropped\n");
        return;
    }
    
    snprintf(tx_buffer, TX_BUFFER_SIZE, ">%s<", response);
    
    tx_busy = true;
    status = nrfx_uarte_tx(&uarte_inst, (uint8_t*)tx_buffer, strlen(tx_buffer), 0);
    if (status != NRFX_SUCCESS) {
        printk("Response TX failed: 0x%08X\n", (unsigned int)status);
        tx_busy = false;
    }
}

uint32_t frequency_to_pause_ms(uint32_t freq_hz)
{
	LOG_DBG("frequency_to_pause_ms(freq=%u)", freq_hz);

	if (freq_hz == 0) {
		LOG_DBG("frequency_to_pause_ms: freq=0, returning 1000 ms");
		return 1000;
	}

	uint32_t period_us = 1000000 / freq_hz;
	uint32_t active_us = timer_get_active_time_us();
	uint32_t pause_us = (period_us > active_us) ? (period_us - active_us) : 0;
	uint32_t result = pause_us / 1000;

	LOG_DBG("frequency_to_pause_ms: period=%u us, active=%u us (pulses=%u), pause=%u ms", 
			period_us, active_us, timer_get_pulse_count(), result);

	return result;
}

uint32_t get_max_frequency(uint32_t pulse_width)
{
	(void)pulse_width;  // Sada koristimo aktuelne parametre iz timer modula
	
	uint32_t result = timer_get_max_frequency_hz();
	
	LOG_DBG("get_max_frequency: active=%u us, pulses=%u, max_freq=%u Hz", 
			timer_get_active_time_us(), timer_get_pulse_count(), result);

	return result;
}

/* ============================================================
 *                    COMMAND HANDLERS
 * ============================================================ */

/**
 * @brief Handler za SON komandu - uključuje stimulaciju
 */
static void handle_son(const char *args)
{
    (void)args;  // Nije potrebno
    
    stimulation_running = true;
    
    if (!timer_system_is_running()) {
        timer_system_start();
        printk("    Action: START stimulation (RUN mode)\n");
        uart_send_response("SONOK");
    } else {
        printk("    Action: Already in RUN mode\n");
        uart_send_response("SONOK");
    }
}

/**
 * @brief Handler za SOFF komandu - isključuje stimulaciju
 */
static void handle_soff(const char *args)
{
    (void)args;  // Nije potrebno
    
    stimulation_running = false;
    
    if (timer_system_is_running()) {
        timer_system_stop();
        printk("    Action: STOP stimulation (STOP mode)\n");
        uart_send_response("OFFOK");
    } else {
        printk("    Action: Already in STOP mode\n");
        uart_send_response("OFFOK");
    }
}

/**
 * @brief Handler za PW komandu - postavlja pulse width
 * @param args HEX string vrednost (1-10)
 */
static void handle_pw(const char *args)
{
    uint8_t pw = (uint8_t)strtoul(args, NULL, 16);
    
    if (pw >= MIN_PULSE_WIDTH && pw <= MAX_PULSE_WIDTH) {
        uint32_t max_freq_new = get_max_frequency(pw);
        if (current_frequency_hz > max_freq_new) {
            LOG_WRN("Pulse width %d reduces max frequency to %u Hz", pw, max_freq_new);
            current_frequency_hz = max_freq_new;
        }
        current_pulse_width = pw;
        printk("    Action: Set Pulse Width = %d (0x%02X)\n", 
               (int)current_pulse_width, (int)current_pulse_width);
        parameters_updated = true;
        uart_send_response("PWOK");
    } else {
        printk("    Action: Pulse Width out of range (%d, 0x%02X)\n", pw, pw);
        uart_send_response("PWERR");
    }
}

/**
 * @brief Handler za SA komandu - postavlja DAC vrednosti za svaki puls
 * @param args Space-separated HEX values (up to 16 DAC values)
 * 
 * Format: SA;00C8 01C2 02BC 03B6 ...  (hex DAC values 0-4095)
 * Each value is a 12-bit DAC value (0x0000 to 0x0FFF).
 * Unlike SC, this does NOT affect the number of active pulses.
 */
static void handle_sa(const char *args)
{
    uint16_t values[16] = {0};
    uint8_t count = 0;
    const char *ptr = args;
    
    LOG_INF("SA command: parsing DAC values");
    
    // Parse up to 16 hex values separated by spaces
    while (*ptr && count < 16) {
        // Skip leading spaces
        while (*ptr == ' ') ptr++;
        if (*ptr == '\0') break;
        
        // Parse hex value
        char *end;
        uint32_t val = strtoul(ptr, &end, 16);
        
        if (end == ptr) {
            // No valid number found
            break;
        }
        
        // Limit to 12-bit DAC range (0-4095)
        values[count] = (uint16_t)(val & 0x0FFF);
        LOG_DBG("DAC[%d] = %u (0x%03X)", count, values[count], values[count]);
        count++;
        ptr = end;
    }
    
    if (count == 0) {
        LOG_WRN("SA: No DAC values parsed");
        uart_send_response("SAERR");
        return;
    }
    
    // Apply DAC values to timer (does NOT change pulse count)
    timer_set_dac_values(values, count);
    
    LOG_INF("SA: Set %d DAC values", count);
    printk("    Action: Set %d DAC values\n", count);
    
    uart_send_response("SAOK");
}

/**
 * @brief Handler za SF komandu - postavlja frekvenciju
 * @param args HEX string vrednost (1-100 Hz)
 */
static void handle_sf(const char *args)
{
    uint8_t freq = (uint8_t)strtoul(args, NULL, 16);
    
    if (freq >= MIN_FREQUENCY_HZ && freq <= MAX_FREQUENCY_HZ) {
        uint32_t max_freq = get_max_frequency(current_pulse_width);
        if (freq > max_freq) {
            LOG_WRN("Frequency %d Hz too high for pulse width %u", freq, (unsigned)current_pulse_width);
            uart_send_response("SFERR");
            return;
        }
        
        LOG_INF("Setting current_frequency_hz from %u to %d", (unsigned)current_frequency_hz, freq);
        current_frequency_hz = freq;
        parameters_updated = true;
        
        uint32_t pause = frequency_to_pause_ms(freq);
        LOG_INF("Frequency set to %d Hz (pause: %u ms)", freq, pause);
        uart_send_response("SFOK");
    } else {
        printk("    Action: Frequency out of range (%d Hz, hex: 0x%02X)\n", freq, freq);
        uart_send_response("SFERR");
    }
}

/**
 * @brief Handler za SC komandu - postavlja MUX patterne
 * @param args Space-separated HEX values (up to 16 patterns)
 * 
 * Format: SC;0100 0004 4000 0020 0001 0800 ...
 * Each value is a 16-bit hex MUX pattern.
 * Patterns with value 0x0000 indicate end of sequence.
 * Number of non-zero patterns determines pulse count.
 */
static void handle_sc(const char *args)
{
    uint16_t patterns[16] = {0};
    uint8_t count = 0;
    const char *ptr = args;
    
    LOG_INF("SC command: parsing patterns");
    
    // Parse up to 16 hex values separated by spaces
    while (*ptr && count < 16) {
        // Skip leading spaces
        while (*ptr == ' ') ptr++;
        if (*ptr == '\0') break;
        
        // Parse hex value
        char *end;
        uint32_t val = strtoul(ptr, &end, 16);
        
        if (end == ptr) {
            // No valid number found
            break;
        }
        
        patterns[count] = (uint16_t)(val & 0xFFFF);
        LOG_DBG("Pattern[%d] = 0x%04X", count, patterns[count]);
        count++;
        ptr = end;
    }
    
    if (count == 0) {
        LOG_WRN("SC: No patterns parsed");
        uart_send_response("SCERR");
        return;
    }
    
    // Apply patterns to timer
    timer_set_mux_patterns(patterns, count);
    
    uint8_t active = timer_get_pulse_count();
    LOG_INF("SC: Set %d patterns, active pulses: %d", count, active);
    printk("    Action: Set %d MUX patterns, active pulses: %d\n", count, active);
    
    uart_send_response("SCOK");
}

/* ============================================================
 *                    COMMAND DISPATCHER
 * ============================================================ */

/**
 * @brief Ispisuje trenutno stanje sistema
 */
static void print_current_state(void)
{
    printk("    Current State: %s, PW=%d(0x%02X), U=%.1fV, F=%uHz(0x%02X)\n\n", 
           stimulation_running ? "RUN" : "STOP",
           (int)current_pulse_width, (int)current_pulse_width,
           voltage_amplitude, 
           (unsigned)current_frequency_hz, (unsigned)current_frequency_hz);
}

/**
 * @brief Parsira i izvršava primljenu komandu prema Sockeet protokolu
 */
static void process_command(const char *cmd)
{
    printk("\n>>> Command received: '%s'\n", cmd);
    
    // Prolazi kroz lookup tabelu i traži odgovarajući handler
    for (size_t i = 0; i < ARRAY_SIZE(cmd_table); i++) {
        if (strncmp(cmd, cmd_table[i].prefix, cmd_table[i].prefix_len) == 0) {
            // Pronađena komanda - pozovi handler
            const char *args = cmd_table[i].has_args ? (cmd + cmd_table[i].prefix_len) : NULL;
            cmd_table[i].handler(args);
            print_current_state();
            return;
        }
    }
    
    // Nije pronađena nijedna komanda
    printk("    Action: Unknown command\n");
    uart_send_response("ERR");
    print_current_state();
}

/* ============================================================
 *                    UART DRIVER - IRQ HANDLER
 * ============================================================ */
static void uarte_handler(nrfx_uarte_event_t const *p_event, void *p_context)
{
    nrfx_err_t status;
    
    switch (p_event->type)
    {
        case NRFX_UARTE_EVT_TX_DONE:
            tx_busy = false;
            break;
            
        case NRFX_UARTE_EVT_RX_DONE:
        {
            char received_char = (char)m_rx_chunk[0];
            
            if (received_char == '>') {
                cmd_started = true;
                cmd_index = 0;
                memset(cmd_buffer, 0, sizeof(cmd_buffer));
            }
            else if (received_char == '<') {
                if (cmd_started && cmd_index > 0) {
                    cmd_buffer[cmd_index] = '\0';
                    process_command(cmd_buffer);
                }
                cmd_started = false;
                cmd_index = 0;
            }
            else if (cmd_started) {
                if (cmd_index < CMD_BUFFER_SIZE - 1) {
                    cmd_buffer[cmd_index++] = received_char;
                } else {
                    printk("ERROR: Command buffer overflow!\n");
                    cmd_started = false;
                    cmd_index = 0;
                }
            }
            
            status = nrfx_uarte_rx(&uarte_inst, m_rx_chunk, RX_CHUNK_SIZE);
            if (status != NRFX_SUCCESS) {
                printk("RX restart failed: 0x%08X\n", (unsigned int)status);
            }
            break;
        }
            
        case NRFX_UARTE_EVT_ERROR:
            printk("[ERROR] 0x%08lX\n", p_event->data.error.error_mask);
            cmd_started = false;
            cmd_index = 0;
            status = nrfx_uarte_rx(&uarte_inst, m_rx_chunk, RX_CHUNK_SIZE);
            if (status != NRFX_SUCCESS) {
                printk("RX restart after error failed: 0x%08X\n", (unsigned int)status);
            }
            break;
            
        default:
            break;
    }
}

/**
 * @brief Timer callback za test komande
 */
static void tx_timer_handler(struct k_timer *timer)
{
    nrfx_err_t status;
    
    if (tx_busy) {
        printk("[TX] BUSY - skipping transmission\n");
        return;
    }
    
    const char* cmd = test_commands[current_cmd_index];
    
    strncpy(tx_buffer, cmd, TX_BUFFER_SIZE - 1);
    tx_buffer[TX_BUFFER_SIZE - 1] = '\0';
    
    printk("[TX] Sending test command: %s\n", tx_buffer);
    
    tx_busy = true;
    status = nrfx_uarte_tx(&uarte_inst, (uint8_t*)tx_buffer, strlen(tx_buffer), 0);
    if (status != NRFX_SUCCESS) {
        printk("TX failed: 0x%08X\n", (unsigned int)status);
        tx_busy = false;
    }
    
    current_cmd_index = (current_cmd_index + 1) % (sizeof(test_commands) / sizeof(test_commands[0]));
}

K_TIMER_DEFINE(tx_timer, tx_timer_handler, NULL);

/* ============================================================
 *                    UART DRIVER - PUBLIC API
 * ============================================================ */

/**
 * @brief Inicijalizuje UART modul
 */
int uart_init(void)
{
    nrfx_err_t status;

#if defined(__ZEPHYR__)
    IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_UARTE_INST_GET(UARTE_INST_IDX)), 
                IRQ_PRIO_LOWEST,
                NRFX_UARTE_INST_HANDLER_GET(UARTE_INST_IDX), 0, 0);
    irq_enable(NRFX_IRQ_NUMBER_GET(NRF_UARTE_INST_GET(UARTE_INST_IDX)));
#endif

    nrfx_uarte_config_t uarte_config = NRFX_UARTE_DEFAULT_CONFIG(UARTE_TX_PIN, UARTE_RX_PIN);
    uarte_config.baudrate = NRF_UARTE_BAUDRATE_115200;
    
    status = nrfx_uarte_init(&uarte_inst, &uarte_config, uarte_handler);
    if (status != NRFX_SUCCESS) {
        printk("ERROR: UART init failed: 0x%08X\n", (unsigned int)status);
        return -1;
    }
    printk("UARTE1 initialized\n");

    memset(cmd_buffer, 0, sizeof(cmd_buffer));
    memset(cathode_channels, 0, sizeof(cathode_channels));
    
    status = nrfx_uarte_rx(&uarte_inst, m_rx_chunk, RX_CHUNK_SIZE);
    if (status != NRFX_SUCCESS) {
        printk("RX start failed: 0x%08X\n", (unsigned int)status);
        return -1;
    }
    printk("RX started\n");

    return 0;
}

/**
 * @brief Šalje string preko UART-a
 */
int uart_send(const char *data)
{
    nrfx_err_t status;
    
    if (tx_busy) {
        return -1;
    }
    
    strncpy(tx_buffer, data, TX_BUFFER_SIZE - 1);
    tx_buffer[TX_BUFFER_SIZE - 1] = '\0';
    
    tx_busy = true;
    status = nrfx_uarte_tx(&uarte_inst, (uint8_t*)tx_buffer, strlen(tx_buffer), 0);
    if (status != NRFX_SUCCESS) {
        tx_busy = false;
        return -1;
    }
    
    return 0;
}

/**
 * @brief Proverava da li je TX zauzet
 */
bool uart_is_tx_busy(void)
{
    return tx_busy;
}

/**
 * @brief Pokreće test timer
 */
void uart_start_test_timer(uint32_t interval_ms)
{
    k_timer_start(&tx_timer, K_MSEC(interval_ms), K_MSEC(interval_ms));
    printk("TX test timer started (%d ms interval)\n", interval_ms);
}

/**
 * @brief Zaustavlja test timer
 */
void uart_stop_test_timer(void)
{
    k_timer_stop(&tx_timer);
    printk("TX test timer stopped\n");
}

/* ============================================================
 *                    PARAMETER GETTERS
 * ============================================================ */

uint32_t uart_get_pause_time_ms(void)
{
	return frequency_to_pause_ms(current_frequency_hz);
}

uint32_t uart_get_frequency_hz(void)
{
	return current_frequency_hz;
}

uint32_t uart_get_pulse_width_ms(void)
{
	return current_pulse_width;
}

uint32_t uart_get_max_frequency(uint32_t pulse_width)
{
	return get_max_frequency(pulse_width);
}

bool uart_parameters_updated(void)
{
	return parameters_updated;
}

void uart_clear_update_flag(void)
{
	parameters_updated = false;
}