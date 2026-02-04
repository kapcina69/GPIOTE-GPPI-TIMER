/**
 * @file ble.c
 * @brief BLE OTA DFU Module
 * 
 * Minimal BLE implementation for Over-The-Air firmware updates only.
 * All parameter control is handled via UART.
 */

#include "ble.h"
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>

#define NRFX_LOG_MODULE BLE
#include <nrfx_log.h>

/* Advertising data - device name only */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

/**
 * @brief Connection callback
 */
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        NRFX_LOG_WARNING("BLE connection failed (err %u)", err);
        return;
    }
    
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    NRFX_LOG_INFO("BLE connected: %s (DFU mode available)", addr);
}

/**
 * @brief Disconnection callback
 */
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    NRFX_LOG_INFO("BLE disconnected: %s (reason %u)", addr, reason);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

int ble_init(void)
{
    int err;
    
    NRFX_LOG_INFO("Initializing BLE for OTA DFU...");
    
    /* Enable Bluetooth */
    err = bt_enable(NULL);
    if (err) {
        NRFX_LOG_ERROR("Bluetooth init failed (err %d)", err);
        return err;
    }
    
    NRFX_LOG_INFO("Bluetooth initialized");
    
    /* Start advertising (SMP DFU service is auto-registered via mcumgr) */
    err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        NRFX_LOG_ERROR("Advertising failed to start (err %d)", err);
        return err;
    }
    
    NRFX_LOG_INFO("BLE OTA DFU ready - Device: %s", DEVICE_NAME);
    
    return 0;
}
