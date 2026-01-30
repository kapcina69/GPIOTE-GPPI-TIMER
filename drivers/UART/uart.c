#include "uart.h"
/*
 * UART command interface (uses Zephyr logging instead of printk)
 */

#include "uart.h"
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

#define UART_NODE DT_NODELABEL(uart0)

static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);

static char uart_buf[UART_BUF_SIZE];
static uint8_t uart_len = 0;
static int64_t last_rx_time = 0;

volatile uint32_t current_frequency_hz = 1;
volatile uint32_t current_pulse_width = 5;
volatile bool parameters_updated = false;

static void uart_process_command(const char *cmd_buffer, uint16_t len);
static void uart_timer_callback(struct k_timer *timer);

K_TIMER_DEFINE(uart_poll_timer, uart_timer_callback, NULL);

uint32_t frequency_to_pause_ms(uint32_t freq_hz)
{
	LOG_DBG("frequency_to_pause_ms(freq=%u)", freq_hz);

	if (freq_hz == 0) {
		LOG_DBG("frequency_to_pause_ms: freq=0, returning 1000 ms");
		return 1000;
	}

	uint32_t period_ms = 1000 / freq_hz;
	uint32_t active_ms = CALCULATE_ACTIVE_TIME_US(current_pulse_width) / 1000;
	uint32_t result = (period_ms > active_ms) ? (period_ms - active_ms) : 0;

	LOG_DBG("frequency_to_pause_ms: period=%u ms, active=%u ms, pause=%u ms", 
			period_ms, active_ms, result);

	return result;
}

uint32_t get_max_frequency(uint32_t pulse_width)
{
	LOG_DBG("get_max_frequency(width=%u)", pulse_width);

	uint32_t active_time_us = CALCULATE_ACTIVE_TIME_US(pulse_width);
	uint32_t period_us = active_time_us + 100;
	uint32_t result = 1000000 / period_us;

	LOG_DBG("get_max_frequency: active=%u us, period=%u us, max_freq=%u Hz", 
			active_time_us, period_us, result);

	return result;
}

int uart_init(void)
{
	LOG_DBG("uart_init: starting");

	if (!device_is_ready(uart_dev)) {
		LOG_ERR("UART device not ready");
		return -1;
	}

	LOG_INF("UART device ready");

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

	k_timer_start(&uart_poll_timer, K_MSEC(5), K_MSEC(5));

	LOG_INF("uart_init complete - UART polling timer started (5ms)");
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
	LOG_DBG("uart_process_command ENTRY: cmd='%s' len=%d", cmd_buffer ? cmd_buffer : "NULL", len);

	if (len == 0) {
		LOG_DBG("Empty command, nothing to do");
		return;
	}

	LOG_INF("Processing command: '%s' (len=%d)", cmd_buffer, len);
	LOG_HEXDUMP_DBG(cmd_buffer, len, "UART_CMD hex dump");
	LOG_DBG("UART_CMD ASCII: %.*s", len, cmd_buffer);

	if (strncmp(cmd_buffer, "SF;", 3) == 0) {
		LOG_INF("SF command detected");
		const char *num_str = &cmd_buffer[3];
		int freq = atoi(num_str);

		LOG_DBG("Parsed frequency: %d", freq);

		if (freq >= MIN_FREQUENCY_HZ && freq <= MAX_FREQUENCY_HZ) {
			uint32_t max_freq = get_max_frequency(current_pulse_width);
			if (freq > max_freq) {
				LOG_WRN("Frequency %d Hz too high for pulse width %u", freq, current_pulse_width);
				uart_send("\r\nERROR: Frequency too high for current pulse width\r\n");
				uart_printf("Maximum frequency: %u Hz\r\n> ", max_freq);
				return;
			}

			LOG_INF("Setting current_frequency_hz from %u to %d", current_frequency_hz, freq);
			current_frequency_hz = freq;
			parameters_updated = true;
			LOG_DBG("parameters_updated=%d", parameters_updated);

			uint32_t pause = frequency_to_pause_ms(freq);
			LOG_INF("Frequency set to %d Hz (pause: %u ms)", freq, pause);
			uart_printf("\r\nOK: Frequency set to %d Hz (pause: %u ms)\r\n> ", freq, pause);
		} else {
			LOG_WRN("Frequency %d OUT OF RANGE [%d-%d]", freq, MIN_FREQUENCY_HZ, MAX_FREQUENCY_HZ);
			uart_printf("\r\nERROR: Frequency out of range [%d-%d]\r\n> ", MIN_FREQUENCY_HZ, MAX_FREQUENCY_HZ);
		}
	} else if (strncmp(cmd_buffer, "SW;", 3) == 0) {
		LOG_INF("SW command detected");
		int width = atoi(&cmd_buffer[3]);
		LOG_DBG("Parsed pulse width: %d", width);

		if (width >= MIN_PULSE_WIDTH && width <= MAX_PULSE_WIDTH) {
			uint32_t max_freq_new = get_max_frequency(width);
			if (current_frequency_hz > max_freq_new) {
				LOG_WRN("Pulse width %d reduces max frequency to %u Hz", width, max_freq_new);
				uart_printf("\r\nWARNING: Frequency auto-adjusted to %u Hz\r\n", max_freq_new);
				current_frequency_hz = max_freq_new;
			}

			LOG_INF("Setting current_pulse_width from %u to %d", current_pulse_width, width);
			current_pulse_width = width;
			parameters_updated = true;
			LOG_INF("Pulse width set to %d (%d µs), max freq: %u Hz", width, width * 100, max_freq_new);
			uart_printf("\r\nOK: Pulse width set to %d (%d µs)\r\n> ", width, width * 100);
		} else {
			LOG_WRN("Pulse width %d out of range [%d-%d]", width, MIN_PULSE_WIDTH, MAX_PULSE_WIDTH);
			uart_printf("\r\nERROR: Pulse width out of range [%d-%d]\r\n> ", MIN_PULSE_WIDTH, MAX_PULSE_WIDTH);
		}
	} else if (strncmp(cmd_buffer, "SA;", 3) == 0) {
		LOG_INF("SA command detected");
		int amplitude = atoi(&cmd_buffer[3]);
		LOG_DBG("Parsed amplitude: %d", amplitude);

		if (amplitude >= 1 && amplitude <= 30) {
			uint16_t dac_value = (uint16_t)(amplitude * 8.5);
			dac_set_value(dac_value);
			LOG_INF("Amplitude set to %d (DAC: %u)", amplitude, dac_value);
			uart_printf("\r\nOK: Amplitude set to %d (DAC: %u)\r\n> ", amplitude, dac_value);
		} else {
			LOG_WRN("Amplitude %d out of range [1-30]", amplitude);
			uart_send("\r\nERROR: Amplitude out of range [1-30]\r\n> ");
		}
	} else if (strncmp(cmd_buffer, "STATUS", 6) == 0 || strncmp(cmd_buffer, "status", 6) == 0) {
		LOG_INF("STATUS command detected");
		uart_send("\r\n--- Current Settings ---\r\n");
		uart_printf("Frequency:   %u Hz\r\n", current_frequency_hz);
		uart_printf("Pulse Width: %u (%u µs)\r\n", current_pulse_width, current_pulse_width * 100);
		uart_printf("Max Freq:    %u Hz\r\n", get_max_frequency(current_pulse_width));
		uart_printf("Param Flag:  %s\r\n", parameters_updated ? "TRUE" : "FALSE");
		uart_send("------------------------\r\n> ");
	} else if (strncmp(cmd_buffer, "HELP", 4) == 0 || strncmp(cmd_buffer, "help", 4) == 0) {
		LOG_INF("HELP command detected");
		uart_send("\r\n--- Available Commands ---\r\n");
		uart_send("SF;<freq>      - Set Frequency (1-100 Hz)\r\n");
		uart_send("SW;<width>     - Set Pulse Width (1-100)\r\n");
		uart_send("SA;<amplitude> - Set Amplitude (1-30)\r\n");
		uart_send("STATUS         - Show current settings\r\n");
		uart_send("HELP           - Show this help\r\n");
		uart_send("--------------------------\r\n> ");
	} else {
		LOG_WRN("Unknown command: '%s'", cmd_buffer);
		uart_send("\r\nERROR: Unknown command. Type HELP for available commands.\r\n> ");
	}

	LOG_INF("Command processing complete");
}

static void uart_timer_callback(struct k_timer *timer)
{
	ARG_UNUSED(timer);
	uart_rx_process();
}

void uart_rx_process(void)
{
	uint8_t c;
	static uint32_t char_count = 0;
	int poll_result;

	poll_result = uart_poll_in(uart_dev, &c);

	/* Process only one character per call */
	if (poll_result == 0) {
		char_count++;

		LOG_DBG("RX char #%u: 0x%02X ('%c'), buffer_len=%d", char_count, c, 
				(c >= 32 && c < 127) ? c : '.', uart_len);

		uart_poll_out(uart_dev, c); /* echo */

		if (c == '\r' || c == '\n') {
			LOG_DBG("ENTER/NEWLINE detected (0x%02X), buffer_len=%d", c, uart_len);

			if (uart_len > 0) {
				uart_buf[uart_len] = '\0';
				LOG_INF("Calling uart_process_command; buf_len=%d", uart_len);
				LOG_DBG("Buffer content: %.*s", uart_len, uart_buf);
				uart_process_command(uart_buf, uart_len);
				LOG_DBG("Back from uart_process_command");
				uart_len = 0;
				LOG_DBG("Buffer cleared, uart_len=0");
			} else {
				LOG_DBG("Empty buffer, ignoring Enter");
			}
		} else if (c == 0x08 || c == 0x7F) {
			if (uart_len > 0) {
				uart_len--;
				uart_send("\b \b"); /* erase character */
				LOG_DBG("Backspace, new_len=%d", uart_len);
			}
		} else if (uart_len < UART_BUF_SIZE - 1) {
			uart_buf[uart_len] = c;
			uart_len++;
			LOG_DBG("Added to buffer[%d] = 0x%02X ('%c'), new_len=%d", 
					uart_len-1, c, c, uart_len);
		} else {
			LOG_WRN("Buffer full! Ignoring character 0x%02X", c);
		}

		last_rx_time = k_uptime_get();
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