# Hardware-Synchronized Pulse Generation System

## Overview

Advanced multi-channel pulse generation system with **UART command control**, hardware-triggered ADC sampling, and SPI-controlled multiplexer. Built on modular driver architecture for nRF52840/nRF52833.

**Key Features:**
- **16-pulse sequential generation** with configurable frequency (1-100 Hz)
- **Dual-timer architecture** with MUX pre-loading for zero-latency switching
- **Hardware event routing (GPPI)** eliminates CPU intervention
- **UART command interface** with loopback testing support
- **Non-blocking UART** - commands processed via work queue
- **Race-condition safe** - atomic operations and IRQ locks
- Synchronized ADC sampling on every pulse
- 16-channel multiplexer control via SPI
- DAC amplitude control via SPI (12-bit, per-pulse configurable)

## Architecture

### Modular Design
```
main.c (orchestration only)
├── services/
│   └── ble.c/h - BLE Nordic UART Service (optional)
├── drivers/
│   ├── UART/ - UART command interface (non-blocking)
│   ├── timers/ - Dual-timer 16-pulse state machine
│   ├── gppi/ - Hardware event routing
│   ├── gpiote/ - Task-controlled output pins
│   ├── saadc/ - Hardware-triggered ADC
│   ├── mux/ - SPI multiplexer control (16-channel)
│   └── dac/ - SPI DAC control (MCP4921, 12-bit)
├── config.h - General project configuration
└── drivers/*/*.config.h - Driver-specific configurations
```

### Hardware Flow
```
Timer State Machine (CC0/CC1)
    ↓ (GPPI)
GPIOTE Tasks → GPIO Pulses (LED1, LED2)
    ↓ (GPPI)
SAADC Sample → ADC Conversion
    ↓ (State Timer ISR)
MUX Pattern Update (SPI) + DAC Value (SPI)
```

## Requirements

| **Board**           | **Support** |
|---------------------|:-----------:|
| nrf52840dk_nrf52840 |     Yes     |
| nrf52833dk_nrf52833 |     Yes     |
| nrf5340dk_nrf5340   |   Possible  |

**SDK:** nRF Connect SDK v2.9.1, Zephyr 3.7.99

## UART Command Protocol

Commands use format: `>CMD;args<`

| Command | Format | Description | Example |
|---------|--------|-------------|---------|
| **SON** | `>SON<` | Start stimulation | `>SON<` → `>SONOK<` |
| **SOFF** | `>SOFF<` | Stop stimulation | `>SOFF<` → `>OFFOK<` |
| **PW** | `>PW;XX<` | Set pulse width (hex, 1-10) | `>PW;5<` → `>PWOK<` |
| **SF** | `>SF;XX<` | Set frequency Hz (hex, 1-100) | `>SF;19<` → `>SFOK<` |
| **SC** | `>SC;XXXX XXXX ...<` | Set MUX patterns (up to 16) | `>SC;0001 0002 0004<` |
| **SA** | `>SA;XXXX XXXX ...<` | Set DAC values (up to 16) | `>SA;0200 0400 0800<` |

### Command Details

**Pulse Width (PW):** Value 1-10, represents 100µs to 1000µs (×100µs)

**Frequency (SF):** Value 1-100 Hz. Validated against max achievable frequency.

**MUX Patterns (SC):** Space-separated 16-bit hex patterns. Number of non-zero patterns determines active pulse count.

**DAC Values (SA):** Space-separated 12-bit hex values (0x000-0xFFF). Does not affect pulse count.

## Pin Configuration

### UART (UARTE1)
| Pin | Function |
|-----|----------|
| P0.06 | TX |
| P0.08 | RX |

### SPI - MUX (SPIM1)
| Pin | Function |
|-----|----------|
| P0.03 | MOSI |
| P0.05 | SCK |
| P0.01 | LE (Latch Enable) |
| P0.00 | CLR (Clear) |

### SPI - DAC (SPIM2)
| Pin | Function |
|-----|----------|
| P0.26 | MOSI |
| P0.27 | SCK |
| P0.28 | CS |

### GPIO Output
| Pin | Function |
|-----|----------|
| P0.13 | LED1 (Pulse output) |
| P0.14 | LED2 (Sequence indicator) |

## Project Structure

### Drivers (`drivers/`)

- **UART/** - UART command interface
  - Non-blocking RX via work queue
  - Race-safe TX with atomic operations
  - Loopback test mode with timer
  - See [drivers/UART/README.md](drivers/UART/README.md)

- **timers/** - Dual-timer pulse generation
  - 16-pulse state machine (configurable 1-16)
  - Dual CC channels: CC0 (state transition), CC1 (MUX pre-load)
  - IRQ-protected pattern/DAC updates
  - See [drivers/timers/README.md](drivers/timers/README.md)
  
- **gppi/** - Hardware event routing
  - 6 GPPI channels for zero-latency triggering
  - Connects timer → GPIOTE, timer → SAADC
  - See [drivers/gppi/README.md](drivers/gppi/README.md)
  
- **gpiote/** - GPIO task mode control
  - Task-controlled output pins
  - Hardware-triggered pulse generation
  - See [drivers/gpiote/README.md](drivers/gpiote/README.md)
  
- **saadc/** - ADC sampling
  - Continuous sampling with double buffering
  - Hardware-triggered on pulse completion
  - See [drivers/saadc/README.md](drivers/saadc/README.md)
  
- **mux/** - SPI multiplexer
  - ISR-safe non-blocking SPI control
  - 16-channel analog routing
  - See [drivers/mux/README.md](drivers/mux/README.md)
  
- **dac/** - SPI DAC control
  - MCP4921 12-bit DAC driver
  - Per-pulse amplitude control
  - See [drivers/dac/README.md](drivers/dac/README.md)

### Services (`services/`)
- **ble.c/h** - Nordic UART Service (optional)
  - BLE commands for remote control
  - See [services/README.md](services/README.md)

## Build and Flash

```bash
cd non_blocking
west build -b nrf52840dk/nrf52840
west flash
```

## Logging

Logs are output via **RTT** (not UART, to keep UART pins free):
```bash
# In VS Code: use nRF Terminal with RTT
# Or via command line:
JLinkRTTClient
```

## Race Condition Protection

The system implements several race-condition safeguards:

1. **Atomic flags** for `tx_busy` and `parameters_updated`
2. **IRQ locks** when updating `mux_patterns[]` and `dac_values[]`
3. **Work queue** for UART command processing (non-blocking ISR)
4. **Atomic test-and-clear** for parameter update flag

## State Machine

```
┌─────────────────────────────────────────────────────────┐
│                    STATE_PULSE                          │
│  Pulse 0 → Pulse 1 → ... → Pulse N-1                   │
│  (N = active_pulse_count, configurable 1-16)           │
│  LED2 = HIGH during entire sequence                     │
└───────────────────────────┬─────────────────────────────┘
                            │ All pulses complete
                            ▼
┌─────────────────────────────────────────────────────────┐
│                    STATE_PAUSE                          │
│  Duration = (1/freq) - active_time                      │
│  LED2 = LOW                                             │
└───────────────────────────┬─────────────────────────────┘
                            │ Pause complete
                            ▼
                      (Repeat cycle)
```

## License

BSD-3-Clause

