/*
 * BLE Communication Module
 * Handles BLE commands for frequency and pulse width control
 */

#include "ble.h"
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/services/nus.h>
#include <string.h>
#include <stdlib.h>

#define NRFX_LOG_MODULE BLE
#include <nrfx_log.h>

// Default values
#define DEFAULT_FREQUENCY_HZ    1
#define DEFAULT_PULSE_WIDTH     5  // 500ms

// Limits
#define MIN_FREQUENCY_HZ        1
#define MAX_FREQUENCY_HZ        100
#define MIN_PULSE_WIDTH         1
#define MAX_PULSE_WIDTH         10

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

// Current settings (volatile jer se mogu menjati iz interrupt konteksta)
static volatile uint32_t current_frequency_hz = DEFAULT_FREQUENCY_HZ;
static volatile uint32_t current_pulse_width = DEFAULT_PULSE_WIDTH;
static volatile bool parameters_updated = false;

// BLE connection handle
static struct bt_conn *current_conn = NULL;

// Advertising data
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
};

/**
 * @brief Calculate maximum allowed frequency based on pulse width
 * max_freq = 1000000 / ((pulse_width * 200 + 100) * 8)
 * This ensures PAUSE is always >= 0
 */
static uint32_t get_max_frequency(uint32_t pulse_width)
{
    if (pulse_width == 0) {
        pulse_width = 1;
    }
    
    uint32_t active_one_pulse_us = pulse_width * 200 + 100;
    uint32_t active_total_us = active_one_pulse_us * 8;  // 8 sequential pulses
    
    if (active_total_us >= 1000000) {
        return 1;  // Minimum 1Hz
    }
    
    uint32_t max_freq = 1000000 / active_total_us;
    return max_freq;
}

/**
 * @brief Calculate pause time from frequency
 * PAUSE = total_period - ACTIVE_duration
 * ACTIVE_duration = (pulse_us * 2 + 100 us overhead) * 8 pulses
 * Koristi preciznu kalkulaciju u µs da izbegne greške zaokruživanja
 */
static uint32_t frequency_to_pause_ms(uint32_t freq_hz)
{
    if (freq_hz == 0) {
        freq_hz = 1;
    }
    
    // Ograniči frekvenciju na maximum dozvoljenu za dati pulse_width
    uint32_t max_allowed_freq = get_max_frequency(current_pulse_width);
    if (freq_hz > max_allowed_freq) {
        freq_hz = max_allowed_freq;
        NRFX_LOG_WARNING("Frequency limited to %u Hz (max for pulse_width=%u)", freq_hz, current_pulse_width);
    }
    
    // Ukupan period u µs (za preciznost)
    uint32_t total_period_us = 1000000 / freq_hz;
    
    // ACTIVE period u µs za JEDNU pulse sekvencu
    // pulse_width je u jedinicama od 100us, pa je pulse_us = pulse_width * 100
    // ACTIVE = pulse_width * 100 * 2 + 100 = pulse_width * 200 + 100 (u us)
    // SA 8 PULSE SEKVENCI: ACTIVE_TOTAL = ACTIVE_one * 8
    uint32_t active_one_pulse_us = current_pulse_width * 200 + 100;
    uint32_t active_period_us = active_one_pulse_us * 8;  // 8 sequential pulses
    
    // PAUSE = total - ACTIVE (u µs, pa konvertuj u ms)
    if (total_period_us > active_period_us) {
        uint32_t pause_us = total_period_us - active_period_us;
        uint32_t pause_ms = pause_us / 1000;  // Precizna konverzija bez round up
        return pause_ms;
    } else {
        // Ako je ACTIVE duže od željenog perioda, vrati 0 (bez pauze)
        // printk("[BLE_CALC] WARNING: ACTIVE period >= total period! Setting PAUSE=0\n");
        return 0;
    }
}

/**
 * @brief Parse and process BLE command
 */
static void ble_process_command(const char *data, uint16_t len)
{
    char cmd_buffer[16];
    
    if (len > sizeof(cmd_buffer) - 1) {
        NRFX_LOG_WARNING("Command too long");
        return;
    }
    
    memcpy(cmd_buffer, data, len);
    cmd_buffer[len] = '\0';
    
    NRFX_LOG_INFO("Received BLE command: %s", cmd_buffer);
    
    // Proveri SF; komandu (Set Frequency)
    if (strncmp(cmd_buffer, "SF;", 3) == 0) {
        int freq = atoi(&cmd_buffer[3]);
        
        printk("[BLE_CMD] Received SF;%d\n", freq);
        
        if (freq >= MIN_FREQUENCY_HZ && freq <= MAX_FREQUENCY_HZ) {
            current_frequency_hz = freq;
            parameters_updated = true;
            printk("[BLE_CMD] Frequency set to %d Hz (pause will be: %d ms)\n", 
                         freq, frequency_to_pause_ms(freq));
            NRFX_LOG_INFO("Frequency set to %d Hz (pause: %d ms)", 
                         freq, frequency_to_pause_ms(freq));
        } else {
            printk("[BLE_CMD] Frequency %d out of range [%d-%d]\n", 
                            freq, MIN_FREQUENCY_HZ, MAX_FREQUENCY_HZ);
            NRFX_LOG_WARNING("Frequency %d out of range [%d-%d]", 
                            freq, MIN_FREQUENCY_HZ, MAX_FREQUENCY_HZ);
        }
    }
    // Proveri SW; komandu (Set Width)
    else if (strncmp(cmd_buffer, "SW;", 3) == 0) {
        int width = atoi(&cmd_buffer[3]);
        
        if (width >= MIN_PULSE_WIDTH && width <= MAX_PULSE_WIDTH) {
            current_pulse_width = width;
            parameters_updated = true;
            NRFX_LOG_INFO("Pulse width set to %d (%d ms)", 
                         width, width * 100);
        } else {
            NRFX_LOG_WARNING("Pulse width %d out of range [%d-%d]", 
                            width, MIN_PULSE_WIDTH, MAX_PULSE_WIDTH);
        }
    }
    else {
        NRFX_LOG_WARNING("Unknown command: %s", cmd_buffer);
    }
}

/**
 * @brief NUS receive callback
 */
static void nus_received_cb(struct bt_conn *conn, const uint8_t *const data, uint16_t len)
{
    NRFX_LOG_INFO("NUS received %d bytes", len);
    ble_process_command((const char *)data, len);
}

/**
 * @brief NUS sent callback
 */
static void nus_sent_cb(struct bt_conn *conn)
{
    NRFX_LOG_INFO("NUS data sent");
}

static struct bt_nus_cb nus_callbacks = {
    .received = nus_received_cb,
    .sent = nus_sent_cb,
};

/**
 * @brief Connection callback
 */
static void connected(struct bt_conn *conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];
    
    if (err) {
        NRFX_LOG_WARNING("Connection failed (err %u)", err);
        return;
    }
    
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    NRFX_LOG_INFO("Connected: %s", addr);
    
    current_conn = bt_conn_ref(conn);
}

/**
 * @brief Disconnection callback
 */
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];
    
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    NRFX_LOG_INFO("Disconnected: %s (reason %u)", addr, reason);
    
    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

int ble_init(void)
{
    int err;
    
    NRFX_LOG_INFO("Initializing BLE module...");
    
    // Enable Bluetooth
    err = bt_enable(NULL);
    if (err) {
        NRFX_LOG_ERROR("Bluetooth init failed (err %d)", err);
        return err;
    }
    
    NRFX_LOG_INFO("Bluetooth initialized");
    
    // Initialize NUS service
    err = bt_nus_init(&nus_callbacks);
    if (err) {
        NRFX_LOG_ERROR("NUS init failed (err %d)", err);
        return err;
    }
    
    NRFX_LOG_INFO("NUS service initialized");
    
    // Start advertising
    err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err) {
        NRFX_LOG_ERROR("Advertising failed to start (err %d)", err);
        return err;
    }
    
    NRFX_LOG_INFO("Advertising successfully started");
    NRFX_LOG_INFO("Device name: %s", DEVICE_NAME);
    NRFX_LOG_INFO("Default frequency: %d Hz", DEFAULT_FREQUENCY_HZ);
    NRFX_LOG_INFO("Default pulse width: %d ms", DEFAULT_PULSE_WIDTH * 100);
    
    return 0;
}

uint32_t ble_get_pause_time_ms(void)
{
    return frequency_to_pause_ms(current_frequency_hz);
}

uint32_t ble_get_frequency_hz(void)
{
    return current_frequency_hz;
}

uint32_t ble_get_pulse_width_ms(void)
{
    return current_pulse_width;
}

uint32_t ble_get_max_frequency(uint32_t pulse_width)
{
    return get_max_frequency(pulse_width);
}

bool ble_parameters_updated(void)
{
    return parameters_updated;
}

void ble_clear_update_flag(void)
{
    parameters_updated = false;
}