/**
 * @file ble.h
 * @brief BLE OTA DFU Module
 * 
 * Minimal BLE interface for Over-The-Air firmware updates only.
 * Parameter control is handled via UART (see drivers/UART/).
 */

#ifndef BLE_H
#define BLE_H

#include <stdint.h>

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

/**
 * @brief Initialize BLE for OTA DFU
 * 
 * Enables Bluetooth, starts advertising. The SMP DFU service
 * is automatically registered via CONFIG_NCS_SAMPLE_MCUMGR_BT_OTA_DFU.
 * 
 * @return 0 on success, negative error code on failure
 */
int ble_init(void);

#endif /* BLE_H */
