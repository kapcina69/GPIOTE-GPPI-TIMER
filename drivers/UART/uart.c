#include "uart.h"
#include "ble.h"
#include <zephyr/drivers/uart.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <zephyr/logging/log.h>
#include "../config.h"
#include "../drivers/dac/dac.h"
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(uart_cmd, LOG_LEVEL_DBG);

/* ================== PRIVATNE DEFINICIJE ================== */

#define UART_NODE DT_NODELABEL(uart0)

/* ================== PRIVATNE PROMENLJIVE ================== */

static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);

static char uart_buf[UART_BUF_SIZE];
static uint8_t uart_len = 0;
static int64_t last_rx_time = 0;

/* ================== JAVNE PROMENLJIVE ================== */

volatile uint32_t current_frequency_hz = 1;
volatile uint32_t current_pulse_width = 5;
volatile bool parameters_updated = false;

/* ================== HELPER MAKROI ================== */


/* ================== PRIVATNE FUNKCIJE - DEKLARACIJE ================== */

static void uart_process_command(const char *cmd_buffer, uint16_t len);
static void uart_timer_callback(struct k_timer *timer);

/* Timer za periodičko procesiranje UART-a */
K_TIMER_DEFINE(uart_poll_timer, uart_timer_callback, NULL);

/* ================== HELPER FUNKCIJE ================== */

uint32_t frequency_to_pause_ms(uint32_t freq_hz)
{
	LOG_DBG(">>> frequency_to_pause_ms: freq_hz=%u", freq_hz);
	printk("[FUNC] frequency_to_pause_ms(freq=%u)\n", freq_hz);
	
	if (freq_hz == 0) {
		LOG_DBG("<<< frequency_to_pause_ms: freq=0, returning 1000 ms");
		printk("[FUNC] Return: 1000 ms (freq=0)\n");
		return 1000;
	}
	
	uint32_t period_ms = 1000 / freq_hz;
	uint32_t active_ms = CALCULATE_ACTIVE_TIME_US(current_pulse_width) / 1000;
	uint32_t result = (period_ms > active_ms) ? (period_ms - active_ms) : 0;
	
	LOG_DBG("<<< frequency_to_pause_ms: period=%u ms, active=%u ms, pause=%u ms", 
	        period_ms, active_ms, result);
	printk("[FUNC] Return: %u ms (period=%u, active=%u)\n", result, period_ms, active_ms);
	
	return result;
}

uint32_t get_max_frequency(uint32_t pulse_width)
{
	LOG_DBG(">>> get_max_frequency: pulse_width=%u", pulse_width);
	printk("[FUNC] get_max_frequency(width=%u)\n", pulse_width);
	
	uint32_t active_time_us = CALCULATE_ACTIVE_TIME_US(pulse_width);
	uint32_t period_us = active_time_us + 100;
	uint32_t result = 1000000 / period_us;
	
	LOG_DBG("<<< get_max_frequency: active=%u us, period=%u us, max_freq=%u Hz", 
	        active_time_us, period_us, result);
	printk("[FUNC] Return: %u Hz (active=%u us, period=%u us)\n", 
	       result, active_time_us, period_us);
	
	return result;
}

/* ================== UART FUNKCIJE ================== */

int uart_init(void)
{
	LOG_DBG(">>> uart_init: starting");
	printk("[FUNC] uart_init() called\n");
	
	if (!device_is_ready(uart_dev)) {
		LOG_ERR("UART device not ready!");
		printk("ERROR: UART device not ready!\n");
		return -1;
	}

	LOG_INF("UART device is ready");
	printk("UART device ready\n");

	uart_send("\r\n=================================\r\n");
	uart_send("UART Command Interface Ready\r\n");
	uart_send("Commands:\r\n");
	uart_send("  SF;<freq>      - Set Frequency (1-100 Hz)\r\n");
	uart_send("  SW;<width>     - Set Pulse Width (1-100)\r\n");
	uart_send("  SA;<amplitude> - Set Amplitude (1-30)\r\n");
	uart_send("  STATUS         - Show current settings\r\n");
	uart_send("  HELP           - Show help\r\n");
	uart_send("=================================\r\n");
	uart_send("NOTE: Press ENTER after typing command!\r\n> ");

	/* Pokreni timer za periodičko procesiranje UART-a */
	k_timer_start(&uart_poll_timer, K_MSEC(5), K_MSEC(5));

	LOG_INF("<<< uart_init: complete");
	printk("[FUNC] uart_init complete\n");
	printk("[FUNC] UART polling timer started (5ms interval)\n");
	return 0;
}

void uart_send(const char *s)
{
	if (!s) {
		LOG_WRN("uart_send: null pointer");
		return;
	}
	
	while (*s) {
		uart_poll_out(uart_dev, *s++);
	}
}

void uart_printf(const char *format, ...)
{
	char buffer[128];
	va_list args;
	
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	
	uart_send(buffer);
}

static void uart_process_command(const char *cmd_buffer, uint16_t len)
{
	LOG_DBG(">>> uart_process_command: ENTRY");
	printk("\n[FUNC] ========================================\n");
	printk("[FUNC] uart_process_command() CALLED\n");
	printk("[FUNC] cmd_buffer='%s', len=%d\n", cmd_buffer ? cmd_buffer : "NULL", len);
	
	if (len == 0) {
		LOG_DBG("Empty command received");
		printk("[FUNC] Empty command, returning\n");
		printk("[FUNC] ========================================\n\n");
		return;
	}
	
	LOG_INF("========================================");
	LOG_INF("Processing command: '%s' (length: %d)", cmd_buffer, len);
	printk("[UART_CMD] Received: '%s' (len=%d)\n", cmd_buffer, len);
	
	/* Debug: ispis svakog karaktera */
	printk("[UART_CMD] Hex dump: ");
	for (int i = 0; i < len; i++) {
		printk("%02X ", (uint8_t)cmd_buffer[i]);
	}
	printk("\n");
	
	/* Debug: ispis ASCII karaktera */
	printk("[UART_CMD] ASCII: '");
	for (int i = 0; i < len; i++) {
		printk("%c", cmd_buffer[i]);
	}
	printk("'\n");
	
	/* SF; komanda (Set Frequency) */
	LOG_DBG("Checking if command starts with 'SF;'...");
	printk("[UART_CMD] Checking for 'SF;' command...\n");
	printk("[UART_CMD] strncmp(cmd_buffer='%s', 'SF;', 3)\n", cmd_buffer);
	
	int cmp_result = strncmp(cmd_buffer, "SF;", 3);
	printk("[UART_CMD] strncmp result: %d (0=match)\n", cmp_result);
	
	if (cmp_result == 0) {
		LOG_INF("*** SF COMMAND DETECTED ***");
		printk("[UART_CMD] *** SF COMMAND DETECTED ***\n");
		
		const char *num_str = &cmd_buffer[3];
		LOG_DBG("Number string: '%s'", num_str);
		printk("[UART_CMD] Number string after 'SF;': '%s'\n", num_str);
		
		int freq = atoi(num_str);
		
		LOG_INF("Parsed frequency: %d Hz", freq);
		printk("[UART_CMD] atoi() returned: %d\n", freq);
		
		LOG_DBG("Checking range: %d <= %d <= %d", MIN_FREQUENCY_HZ, freq, MAX_FREQUENCY_HZ);
		printk("[UART_CMD] Range check: %d <= %d <= %d\n", MIN_FREQUENCY_HZ, freq, MAX_FREQUENCY_HZ);
		
		if (freq >= MIN_FREQUENCY_HZ && freq <= MAX_FREQUENCY_HZ) {
			LOG_INF("Frequency is in valid range");
			printk("[UART_CMD] Frequency IN RANGE\n");
			
			LOG_DBG("Getting max frequency for pulse width %u", current_pulse_width);
			printk("[UART_CMD] Calling get_max_frequency(%u)...\n", current_pulse_width);
			
			uint32_t max_freq = get_max_frequency(current_pulse_width);
			LOG_INF("Max frequency for pulse width %u: %u Hz", current_pulse_width, max_freq);
			printk("[UART_CMD] Max freq = %u Hz\n", max_freq);
			
			LOG_DBG("Checking if freq %d > max_freq %u", freq, max_freq);
			printk("[UART_CMD] Checking: %d > %u?\n", freq, max_freq);
			
			if (freq > max_freq) {
				LOG_WRN("Frequency %d Hz too high for pulse width %u", freq, current_pulse_width);
				printk("[UART_CMD] ERROR: Freq TOO HIGH\n");
				uart_send("\r\nERROR: Frequency too high for current pulse width\r\n");
				uart_printf("Maximum frequency: %u Hz\r\n> ", max_freq);
				printk("[FUNC] ========================================\n\n");
				return;
			}
			
			LOG_INF("Setting current_frequency_hz from %u to %d", current_frequency_hz, freq);
			printk("[UART_CMD] OLD freq: %u Hz\n", current_frequency_hz);
			
			current_frequency_hz = freq;
			
			printk("[UART_CMD] NEW freq: %u Hz\n", current_frequency_hz);
			LOG_INF("current_frequency_hz updated to: %u", current_frequency_hz);
			
			LOG_INF("Setting parameters_updated flag to TRUE");
			printk("[UART_CMD] Setting parameters_updated = TRUE\n");
			
			parameters_updated = true;
			
			printk("[UART_CMD] parameters_updated = %s\n", parameters_updated ? "TRUE" : "FALSE");
			
			LOG_DBG("Calling frequency_to_pause_ms(%d)", freq);
			printk("[UART_CMD] Calculating pause...\n");
			
			uint32_t pause = frequency_to_pause_ms(freq);
			
			LOG_INF("✓✓✓ Frequency set to %d Hz (pause: %u ms) ✓✓✓", freq, pause);
			printk("[UART_CMD] ✓✓✓ SUCCESS ✓✓✓\n");
			printk("[UART_CMD] Frequency: %d Hz\n", freq);
			printk("[UART_CMD] Pause: %u ms\n", pause);
			printk("[UART_CMD] Flag: %s\n", parameters_updated ? "SET" : "NOT SET");
			
			uart_printf("\r\nOK: Frequency set to %d Hz (pause: %u ms)\r\n> ", freq, pause);
		} else {
			LOG_WRN("Frequency %d OUT OF RANGE [%d-%d]", freq, MIN_FREQUENCY_HZ, MAX_FREQUENCY_HZ);
			printk("[UART_CMD] ERROR: Freq OUT OF RANGE\n");
			uart_printf("\r\nERROR: Frequency out of range [%d-%d]\r\n> ",
			           MIN_FREQUENCY_HZ, MAX_FREQUENCY_HZ);
		}
	}
	/* SW; komanda (Set Width) */
	else if (strncmp(cmd_buffer, "SW;", 3) == 0) {
		LOG_INF("*** SW COMMAND DETECTED ***");
		printk("[UART_CMD] *** SW COMMAND DETECTED ***\n");
		
		int width = atoi(&cmd_buffer[3]);
		LOG_INF("Parsed pulse width: %d", width);
		printk("[UART_CMD] Parsed width: %d\n", width);
		
		if (width >= MIN_PULSE_WIDTH && width <= MAX_PULSE_WIDTH) {
			uint32_t max_freq_new = get_max_frequency(width);
			LOG_DBG("New max frequency: %u Hz", max_freq_new);
			
			if (current_frequency_hz > max_freq_new) {
				LOG_WRN("Pulse width %d reduces max frequency to %u Hz", width, max_freq_new);
				printk("[UART_CMD] WARNING: Auto-adjusting freq to %u Hz\n", max_freq_new);
				uart_printf("\r\nWARNING: Frequency auto-adjusted to %u Hz\r\n", max_freq_new);
				current_frequency_hz = max_freq_new;
			}
			
			LOG_INF("Setting current_pulse_width from %u to %d", current_pulse_width, width);
			current_pulse_width = width;
			parameters_updated = true;
			
			LOG_INF("✓ Pulse width set to %d (%d µs), max freq: %u Hz", width, width * 100, max_freq_new);
			printk("[UART_CMD] ✓ Pulse width set: %d (%d µs)\n", width, width * 100);
			printk("[UART_CMD] parameters_updated = TRUE\n");
			
			uart_printf("\r\nOK: Pulse width set to %d (%d µs)\r\n> ", width, width * 100);
		} else {
			LOG_WRN("Pulse width %d out of range [%d-%d]", width, MIN_PULSE_WIDTH, MAX_PULSE_WIDTH);
			printk("[UART_CMD] ERROR: Width out of range\n");
			uart_printf("\r\nERROR: Pulse width out of range [%d-%d]\r\n> ",
			           MIN_PULSE_WIDTH, MAX_PULSE_WIDTH);
		}
	}
	/* SA; komanda (Set Amplitude) */
	else if (strncmp(cmd_buffer, "SA;", 3) == 0) {
		LOG_INF("*** SA COMMAND DETECTED ***");
		printk("[UART_CMD] *** SA COMMAND DETECTED ***\n");
		
		int amplitude = atoi(&cmd_buffer[3]);
		LOG_INF("Parsed amplitude: %d", amplitude);
		printk("[UART_CMD] Parsed amplitude: %d\n", amplitude);
		
		if (amplitude >= 1 && amplitude <= 30) {
			uint16_t dac_value = (uint16_t)(amplitude * 8.5);
			dac_set_value(dac_value);
			
			LOG_INF("✓ Amplitude set to %d (DAC: %u)", amplitude, dac_value);
			printk("[UART_CMD] ✓ Amplitude set: %d (DAC: %u)\n", amplitude, dac_value);
			
			uart_printf("\r\nOK: Amplitude set to %d (DAC: %u)\r\n> ", amplitude, dac_value);
		} else {
			LOG_WRN("Amplitude %d out of range [1-30]", amplitude);
			printk("[UART_CMD] ERROR: Amplitude out of range\n");
			uart_send("\r\nERROR: Amplitude out of range [1-30]\r\n> ");
		}
	}
	/* STATUS komanda */
	else if (strncmp(cmd_buffer, "STATUS", 6) == 0 || strncmp(cmd_buffer, "status", 6) == 0) {
		LOG_INF("*** STATUS COMMAND DETECTED ***");
		printk("[UART_CMD] *** STATUS COMMAND DETECTED ***\n");
		
		uart_send("\r\n--- Current Settings ---\r\n");
		uart_printf("Frequency:   %u Hz\r\n", current_frequency_hz);
		uart_printf("Pulse Width: %u (%u µs)\r\n", current_pulse_width, current_pulse_width * 100);
		uart_printf("Max Freq:    %u Hz\r\n", get_max_frequency(current_pulse_width));
		uart_printf("Param Flag:  %s\r\n", parameters_updated ? "TRUE" : "FALSE");
		uart_send("------------------------\r\n> ");
	}
	/* HELP komanda */
	else if (strncmp(cmd_buffer, "HELP", 4) == 0 || strncmp(cmd_buffer, "help", 4) == 0) {
		LOG_INF("*** HELP COMMAND DETECTED ***");
		printk("[UART_CMD] *** HELP COMMAND DETECTED ***\n");
		
		uart_send("\r\n--- Available Commands ---\r\n");
		uart_send("SF;<freq>      - Set Frequency (1-100 Hz)\r\n");
		uart_send("SW;<width>     - Set Pulse Width (1-100)\r\n");
		uart_send("SA;<amplitude> - Set Amplitude (1-30)\r\n");
		uart_send("STATUS         - Show current settings\r\n");
		uart_send("HELP           - Show this help\r\n");
		uart_send("--------------------------\r\n> ");
	}
	else {
		LOG_WRN("*** UNKNOWN COMMAND: '%s' ***", cmd_buffer);
		printk("[UART_CMD] *** UNKNOWN COMMAND ***\n");
		uart_send("\r\nERROR: Unknown command. Type HELP for available commands.\r\n> ");
	}
	
	LOG_INF("Command processing complete");
	LOG_INF("========================================");
	printk("[FUNC] Processing complete\n");
	printk("[FUNC] ========================================\n\n");
}

/**
 * @brief Timer callback - poziva se svakih 5ms za procesiranje UART-a
 * NON-BLOCKING: Procesira SAMO JEDAN karakter po pozivu!
 */
static void uart_timer_callback(struct k_timer *timer)
{
	uart_rx_process();
}

/**
 * @brief Procesira UART prijem - NON-BLOCKING verzija
 * Procesira SAMO JEDAN karakter po pozivu!
 */
void uart_rx_process(void)
{
	uint8_t c;
	static uint32_t char_count = 0;
	int poll_result;

	poll_result = uart_poll_in(uart_dev, &c);
	
	/* KRITIČNA IZMENA: IF umesto WHILE - procesira samo JEDAN karakter! */
	if (poll_result == 0) {
		char_count++;
		
		LOG_DBG("RX char #%u: 0x%02X ('%c')", char_count, c, 
		        (c >= 32 && c < 127) ? c : '.');
		printk("[UART_RX] Received: 0x%02X ('%c'), buffer_len=%d\n", 
		       c, (c >= 32 && c < 127) ? c : '.', uart_len);
		
		uart_poll_out(uart_dev, c); /* echo */

		/* Provera za Enter/Newline - izvršavanje komande */
		if (c == '\r' || c == '\n') {
			LOG_DBG("*** ENTER/NEWLINE DETECTED ***");
			printk("[UART_RX] *** ENTER/NEWLINE (0x%02X) DETECTED ***\n", c);
			printk("[UART_RX] Current buffer_len=%d\n", uart_len);
			
			if (uart_len > 0) {
				uart_buf[uart_len] = '\0';
				
				LOG_INF("*** CALLING uart_process_command ***");
				printk("[UART_RX] *** CALLING uart_process_command ***\n");
				printk("[UART_RX] Buffer content: '%s'\n", uart_buf);
				printk("[UART_RX] Buffer length: %d\n", uart_len);
				
				uart_process_command(uart_buf, uart_len);
				
				LOG_DBG("*** BACK FROM uart_process_command ***");
				printk("[UART_RX] *** BACK FROM uart_process_command ***\n");
				
				uart_len = 0;
				
				LOG_DBG("Buffer cleared");
				printk("[UART_RX] Buffer cleared, uart_len=0\n");
			} else {
				LOG_DBG("Empty buffer, ignoring Enter");
				printk("[UART_RX] Empty buffer, ignoring Enter\n");
			}
		}
		/* Backspace */
		else if (c == 0x08 || c == 0x7F) {
			if (uart_len > 0) {
				uart_len--;
				uart_send("\b \b"); /* erase character */
				LOG_DBG("Backspace, new length: %d", uart_len);
				printk("[UART_RX] Backspace, new_len=%d\n", uart_len);
			}
		}
		/* Dodaj karakter u buffer */
		else if (uart_len < UART_BUF_SIZE - 1) {
			uart_buf[uart_len] = c;
			uart_len++;
			LOG_DBG("Added to buffer[%d] = 0x%02X ('%c'), new length: %d", 
			        uart_len-1, c, c, uart_len);
			printk("[UART_RX] buffer[%d] = 0x%02X, new_len=%d\n", uart_len-1, c, uart_len);
		} else {
			LOG_WRN("Buffer full! Ignoring character 0x%02X", c);
			printk("[UART_RX] WARNING: Buffer FULL! Ignoring 0x%02X\n", c);
		}
		
		last_rx_time = k_uptime_get();
		
		/* KRITIČNA IZMENA: NEMA VIŠE PETLJE - vraća kontrolu nakon JEDNOG karaktera! */
	}
}

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