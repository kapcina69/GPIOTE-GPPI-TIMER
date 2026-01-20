# GPIOTE-GPPI-TIMER: Advanced SAADC Trigger with Precision Timing

Professional embedded firmware demonstrating synchronized ADC sampling, PWM pulse generation, and event-driven architecture using Nordic nRF52840 GPIOTE, GPPI (GPIO PPI), and Timer peripherals.

## 📋 Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
- [Hardware Requirements](#hardware-requirements)
- [Software Requirements](#software-requirements)
- [Building and Running](#building-and-running)
- [Configuration](#configuration)
- [BLE Commands](#ble-commands)
- [Performance](#performance)
- [Technical Details](#technical-details)

## Overview

This project implements a **multi-timer event-driven system** that generates synchronized PWM pulses while simultaneously sampling an analog-to-digital converter (SAADC) at precise moments. The architecture eliminates CPU polling by using hardware-triggered tasks connected through GPPI (Generalized Programmable Peripheral Interconnect).

**Key Achievement**: Achieved exact frequency accuracy by calculating PAUSE timing in microseconds domain rather than milliseconds, eliminating rounding errors. For 100Hz operation: PAUSE = 8.9ms (exactly 8900µs) instead of previous 8.7ms.

## Features

✅ **SAADC Batched Sampling** - Batches ADC interrupts (default 10 samples per wake)  
✅ **GPPI Hardware Triggering** - TIMER1 CC1 event triggers ADC sampling  
✅ **Precision Timing** - Microsecond-level calculation, no rounding errors  
✅ **Event-Driven Architecture** - Main loop sleeps with k_sleep(K_FOREVER)  
✅ **Dual LED Control** - Two independent pulse trains via GPIOTE  
✅ **BLE Parameter Control** - Wireless frequency/pulse width adjustment (1-100Hz)  
✅ **State Machine** - Alternating ACTIVE/PAUSE periods with precise timing  
✅ **Statistics Monitoring** - Real-time sample count and state transitions  

## Architecture

### Timer Configuration

| Timer | Index | Purpose | Base Freq | Mode |
|-------|-------|---------|-----------|------|
| TIMER1 | 1 | Pulse Generation (LED control) | 16MHz | Compare with 6 CC channels |
| TIMER2 | 2 | State Machine (ACTIVE/PAUSE) | 16MHz | Compare single channel |
| TIMER0 | 3 | Dormant (legacy, not used) | - | - |

### GPPI Channel Routing

| Ch | Source Event | Task Destination | Purpose |
|----|-------------|-----------------|---------|
| 15 | TIMER1 CC0 | PIN1_CLR | LED1 activation (LOW) |
| 14 | TIMER1 CC1 | PIN1_SET | LED1 deactivation (HIGH) + **ADC TRIGGER** |
| 13 | TIMER1 CC2 | PIN2_CLR | LED2 activation (LOW) |
| 12 | TIMER1 CC3 | PIN2_SET | LED2 deactivation (HIGH) |
| 11 | TIMER1 CC1 | SAADC_SAMPLE | **ADC external trigger** (synchronized with LED) |
| 10 | SAADC_END | TIMER1_CAPTURE4 | Timing measurement |

### SAADC Configuration

```
Mode: Advanced with external triggers only
Trigger Source: GPPI channel 11 (TIMER1 CC1 event)
Resolution: 10-bit
Input: AIN0
Buffer: Batched buffer (ADC_INTERRUPT_BATCH_SIZE, default 10 samples)
Auto Re-arm: start_on_end = true
```

### State Machine

```
┌─────────────┐
│   ACTIVE    │  PWM pulsing for pulse_us * 2 + 100µs
│ (LED ON)    │  TIMER1 running, SAADC triggered
└──────┬──────┘
       │ (TIMER2 expires)
       ▼
┌─────────────┐
│    PAUSE    │  No pulsing for (1000000/freq_hz - active_period_us) µs
│ (LED OFF)   │  TIMER1 disabled, SAADC idle
└──────┬──────┘
       │ (TIMER2 expires)
       ▼
    (repeat)
```

## Hardware Requirements

**Board**: nRF52840dk (other nRF52/nRF53 boards supported with minimal changes)

**Connectors**:
- LED1 (P0.13) - Primary pulse output
- LED2 (P0.14) - Secondary pulse output  
- AIN0 (P0.02) - ADC analog input
- UART (TX=P0.06, RX=P0.08) - Debug/BLE serial output

**Power**: USB or external 3.3V

## Software Requirements

- **nRF Connect SDK**: v2.9.1
- **Zephyr**: 3.7.99
- **Python**: 3.8+ (for serial monitoring)
- **ARM GCC**: Included in SDK toolchain

## Building and Running

### Prerequisites

```bash
# Install nRF Connect SDK (if not already installed)
git clone https://github.com/nrfconnect/sdk-nrf.git nrf_sdk
cd nrf_sdk
west init -m https://github.com/nrfconnect/sdk-nrf --mr v2.9.1
west update
```

### Build

```bash
cd c:\ncs\v2.9.1\modules\hal\nordic\nrfx\samples\src\nrfx_gppi\fork
west build -d build -b nrf52840dk/nrf52840
```

### Flash

```bash
west flash
```

### Monitor Serial Output

```bash
# Option 1: Using west
west esputil monitor

# Option 2: Using Python serial monitor
python -m serial.tools.miniterm /dev/ttyACM0 115200

# Option 3: Using screen (macOS/Linux)
screen /dev/ttyACM0 115200
```

Expected output (low-power defaults, stats timer disabled):
```
=== ULTRA LOW POWER MODE ===
ADC batch size: 10 samples
Stats timer: OFF
SAADC batched mode initialized
GPPI channels enabled (ADC trigger on TIMER1 CC1)
System started - LOW POWER MODE

=== LOW POWER CONFIGURATION ===
ADC interrupt batching: 10 samples
Stats timer: DISABLED
Expected CPU wake-ups per second:
  - ADC: ~10 Hz (batched)
  - State: ~200 Hz
  - Stats: 0 Hz
  - BLE: variable
```

## Configuration

### Runtime Parameters (via BLE)

Use Nordic BLE terminal app or custom client to send:

**Set Frequency**: `SF;X` where X = 1-100 (Hz)
```
SF;50    # Set to 50Hz
SF;100   # Set to 100Hz
```

**Set Pulse Width**: `SW;X` where X = 1-10 (represents 100-1000µs)
```
SW;5     # 500µs pulse width
SW;3     # 300µs pulse width
```

### Compile-Time Configuration (prj.conf)

```
# Logging
CONFIG_LOG=y
CONFIG_PRINTK=y
CONFIG_LOG_MODE_IMMEDIATE=y
CONFIG_LOG_BUFFER_SIZE=4096

# BLE (Nordic UART Service)
CONFIG_BT=y
CONFIG_BT_DEVICE_NAME="ADC-Trigger-Sample"
CONFIG_BT_NUS=y

# Zephyr
CONFIG_KERNEL_TIMERS=y
```

## BLE Commands

| Command | Format | Range | Example | Result |
|---------|--------|-------|---------|--------|
| Set Frequency | SF;X | 1-100 Hz | SF;100 | Sets 100Hz (10ms period) |
| Set Pulse Width | SW;X | 1-10 | SW;5 | Sets 500µs pulse width |

The system validates inputs and ignores invalid commands.

## Performance

### Timing Accuracy (100Hz Operation)

```
Total Period:    10000µs (10ms)
ACTIVE Duration: 1100µs (pulse_width*2 + 100 overhead)
PAUSE Duration:  8900µs (calculated = total - active)

Achieved Precision: ±0 rounding error (microsecond domain calculation)
```

### Memory Usage

```
FLASH: 162.7 KB (15.5% of 1024KB)
  - Code: 162 KB
  - LTO: Enabled (Link-Time Optimization)

RAM: 33.4 KB (12.7% of 256KB)
  - Zephyr kernel: ~8 KB
  - BLE stack: ~12 KB
  - Application data: ~13 KB
```

### Sampling Rate

```
At 100Hz PULSE frequency:
- ADC samples triggered: 1 per PULSE (CC1 event)
- Samples captured: 100 samples/second (exact frequency)
- CPU wake-ups from ADC: 10/sec with default batch size (10 samples per ISR)

At higher frequencies, ADC can still batch; adjust `ADC_INTERRUPT_BATCH_SIZE` for latency vs. power
```

## Technical Details

### SAADC Advanced Mode with External Triggers

```c
// Configuration
nrfx_saadc_advanced_mode_set(channels_mask,
    SAADC_RESOLUTION,
    &(nrfx_saadc_adv_config_t){
        .oversampling = NRF_SAADC_OVERSAMPLE_DISABLED,
        .burst = NRF_SAADC_BURST_DISABLED,
        .internal_timer_cc = 0,
        .start_on_end = true,  // Auto re-arm for continuous triggering
    },
    saadc_handler);
```

**Critical Sequence**:
1. `nrf_saadc_enable()` - Enable SAADC register
2. `nrfx_saadc_mode_trigger()` - ARM for external triggers
3. GPPI routes TIMER1 CC1 event → SAADC SAMPLE task
4. Each TIMER1 CC1 comparison triggers ADC sampling

### Microsecond Precision Calculation

**Old (Lossy) Approach**:
```c
pause_ms = (1000 / freq_hz) - (active_period_us + 999) / 1000;
// 100Hz: 10 - 2 = 8ms ❌ (lost 900µs)
```

**New (Precise) Approach**:
```c
uint32_t total_period_us = 1000000 / freq_hz;      // 10000µs
uint32_t pause_us = total_period_us - active_period_us;  // 8900µs
uint32_t pause_ticks = nrfx_timer_us_to_ticks(&timer_state, pause_us);
// Hardware conversion with nanosecond precision ✅
```

### Event-Driven Interrupt Handlers

All work offloaded from main loop to interrupt context:

```
TIMER2 (state machine) → ACTIVE/PAUSE transitions
SAADC (DONE handler) → Processes batch completion every N samples (default 10)
SAADC (FINISHED handler) → Restart ADC immediately
Zephyr Timer → Optional stats every 1s (disabled by default for power)
```

Main loop: `k_sleep(K_FOREVER)` - CPU available for BLE stack

## File Structure

```
├── main.c              # Core application (759 lines)
├── ble.c               # BLE UART service (260 lines)
├── ble.h               # BLE API header
├── CMakeLists.txt      # Build configuration
├── prj.conf            # Kconfig settings
├── sample.yaml         # Zephyr sample metadata
└── README.md           # This file
```

## Troubleshooting

| Issue | Cause | Solution |
|-------|-------|----------|
| ADC not sampling | SAADC not armed | Verify `nrfx_saadc_mode_trigger()` called before timer starts |
| Wrong frequency | Rounding in millisecond domain | Ensure microsecond-based calculation in ACTIVE→PAUSE transition |
| BLE commands ignored | Buffer overflow | Increase `CONFIG_LOG_BUFFER_SIZE` to 4096 |
| Missing samples | Batch buffer not re-armed | Ensure `nrfx_saadc_buffer_set(...)` is called in BUF_REQ/FINISHED |

## References

- [nRF52840 Datasheet](https://infocenter.nordicsemi.com/pdf/nRF52840_PS_v3.4.pdf)
- [nRFx Drivers Documentation](https://github.com/NordicSemiconductor/nrfx)
- [Zephyr RTOS Documentation](https://docs.zephyrproject.org/)
- [nRF Connect SDK](https://developer.nordicsemi.com/nRF_Connect_SDK/)

## License

Licensed under the Nordic License. See LICENSE file for details.

## Author

**Development Date**: January 2026  
**Platform**: nRF52840dk with nRF Connect SDK v2.9.1  
**Optimization Focus**: Precision timing, event-driven architecture, zero-jitter GPPI routing

[//]: #
[Building and running]: <../../../README.md#building-and-running>
