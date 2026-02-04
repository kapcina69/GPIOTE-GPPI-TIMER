# BLE OTA DFU Service

## Overview

Minimal BLE implementation for Over-The-Air (OTA) firmware updates only. All runtime parameter control is handled via hardware UART.

## Purpose

- **OTA Firmware Updates**: Upload new firmware via BLE using nRF Connect or mcumgr
- **No Runtime Commands**: Parameter control moved to UART (see [drivers/UART/](../drivers/UART/))

## Features

- MCUmgr SMP protocol for DFU
- Minimal BLE footprint (no NUS service)
- Low power advertising
- Auto-reconnect after DFU

## Configuration

In `prj.conf`:
```
CONFIG_BT=y
CONFIG_BT_PERIPHERAL=y
CONFIG_BT_DEVICE_NAME="Nordic_Timer"
CONFIG_BT_NUS=n                           # Disabled - use UART instead
CONFIG_NCS_SAMPLE_MCUMGR_BT_OTA_DFU=y     # Enable OTA DFU
```

## API

### Initialization
```c
int ble_init(void);
```
Initializes Bluetooth, starts advertising. SMP DFU service is auto-registered.

## OTA Update Process

### Using nRF Connect App (Android/iOS)

1. Build with `west build`
2. Open nRF Connect app
3. Connect to "Nordic_Timer"
4. Go to DFU tab
5. Select `app_update.bin` from `build/zephyr/`
6. Start upload

### Using mcumgr CLI

```bash
# List images
mcumgr --conntype ble --connstring ctlr_name=hci0,peer_name='Nordic_Timer' image list

# Upload new image
mcumgr --conntype ble --connstring ctlr_name=hci0,peer_name='Nordic_Timer' image upload build/zephyr/app_update.bin

# Confirm and reset
mcumgr --conntype ble --connstring ctlr_name=hci0,peer_name='Nordic_Timer' reset
```

## Memory Layout

DFU requires MCUboot bootloader with dual-slot configuration:
```
┌─────────────────┐ 0x00000000
│    MCUboot      │ (Bootloader)
├─────────────────┤ 0x0000C000
│   Slot 0        │ (Active image)
│   (Application) │
├─────────────────┤ 0x0007E000
│   Slot 1        │ (Update image)
│   (Staging)     │
├─────────────────┤ 0x000F0000
│   Settings      │
└─────────────────┘ 0x00100000
```

## Build Artifacts for DFU

After `west build`:
- `build/zephyr/app_update.bin` - Signed update image
- `build/zephyr/zephyr.signed.hex` - For J-Link programming
- `build/zephyr/merged.hex` - Bootloader + app combined

## Dependencies

- Zephyr BLE stack
- MCUmgr (via `CONFIG_NCS_SAMPLE_MCUMGR_BT_OTA_DFU`)
- MCUboot bootloader
