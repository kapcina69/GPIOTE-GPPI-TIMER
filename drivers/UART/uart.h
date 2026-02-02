#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stdbool.h>
#include "../config.h"
#include "../drivers/dac/dac.h"

// UART konfiguracija
#define UARTE_INST_IDX 1
#define UARTE_TX_PIN 16
#define UARTE_RX_PIN 15
#define RX_CHUNK_SIZE 1
#define CMD_BUFFER_SIZE 128
#define TX_BUFFER_SIZE 128
#define UART_BUF_SIZE            32
#define UART_RX_TIMEOUT_MS       100

// Default values
#define DEFAULT_FREQUENCY_HZ    1
#define DEFAULT_PULSE_WIDTH     5  // 500ms

// Limits
#define MIN_FREQUENCY_HZ        1
#define MAX_FREQUENCY_HZ        100
#define MIN_PULSE_WIDTH         1
#define MAX_PULSE_WIDTH         10

/* ================== JAVNE PROMENLJIVE ================== */

extern volatile uint32_t current_frequency_hz;
extern volatile uint32_t current_pulse_width;
extern volatile bool parameters_updated;
/**
 * @brief Inicijalizuje UART modul
 * @return 0 na uspeh, negativna vrednost na grešku
 */
int uart_init(void);

/**
 * @brief Šalje string preko UART-a
 * @param data String za slanje
 * @return 0 na uspeh, negativna vrednost na grešku
 */
int uart_send(const char *data);

/**
 * @brief Šalje formatiran odgovor (>response<)
 * @param response Sadržaj odgovora (bez > i <)
 */
void uart_send_response(const char *response);

/**
 * @brief Proverava da li je TX zauzet
 * @return true ako je TX u toku
 */
bool uart_is_tx_busy(void);

/**
 * @brief Pokreće test timer za periodično slanje komandi
 * @param interval_ms Interval u milisekundama
 */
void uart_start_test_timer(uint32_t interval_ms);

/**
 * @brief Zaustavlja test timer
 */
void uart_stop_test_timer(void);

uint32_t frequency_to_pause_ms(uint32_t freq_hz);

uint32_t uart_get_pause_time_ms(void);

/**
 * @brief Get current frequency in Hz
 * 
 * @return Frequency in Hz
 */
uint32_t uart_get_frequency_hz(void);

/**
 * @brief Get current pulse width in milliseconds
 * 
 * @return Pulse width in ms
 */
uint32_t uart_get_pulse_width_ms(void);

/**
 * @brief Get maximum allowed frequency based on pulse width
 * Ensures that PAUSE >= 0 for 8 sequential pulses
 * 
 * @param pulse_width Pulse width in units of 100µs
 * @return Maximum frequency in Hz
 */
uint32_t uart_get_max_frequency(uint32_t pulse_width);

/**
 * @brief Check if parameters have been updated since last check
 * 
 * @return true if parameters changed, false otherwise
 */
bool uart_parameters_updated(void);

/**
 * @brief Clear the updated flag after reading new parameters
 */
void uart_clear_update_flag(void);

#endif /* UART_H */
