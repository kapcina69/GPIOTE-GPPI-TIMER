#ifndef UART_H
#define UART_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include "../config.h"
#include "../drivers/dac/dac.h"
/* ================== DEFINICIJE ================== */

#define UART_BUF_SIZE            32
#define UART_RX_TIMEOUT_MS       100



/* ================== JAVNE PROMENLJIVE ================== */

extern volatile uint32_t current_frequency_hz;
extern volatile uint32_t current_pulse_width;
extern volatile bool parameters_updated;

/* ================== JAVNE FUNKCIJE ================== */

/**
 * @brief Inicijalizuje UART modul
 * @return 0 ako je uspešno, negativan broj ako nije
 */
int uart_init(void);

/**
 * @brief Šalje string preko UART-a
 * @param s Pointer na null-terminated string
 */
void uart_send(const char *s);

/**
 * @brief Formatira i šalje string preko UART-a (kao printf)
 * @param format Format string
 * @param ... Argumenti
 */
void uart_printf(const char *format, ...);

/**
 * @brief Procesira primljene podatke sa UART-a
 * Poziva se periodično iz main loop-a
 */
void uart_rx_process(void);

/**
 * @brief Kalkuliše maksimalnu frekvenciju za zadatu širinu pulsa
 * @param pulse_width Širina pulsa
 * @return Maksimalna frekvencija u Hz
 */
uint32_t get_max_frequency(uint32_t pulse_width);

/**
 * @brief Kalkuliše pauzu u milisekundama na osnovu frekvencije
 * @param freq_hz Frekvencija u Hz
 * @return Pauza u milisekundama
 */
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