# GPPI Driver

## Overview

GPPI (Generic Peripheral Interconnect) driver for hardware-triggered pulse generation and ADC sampling. Connects timer events to GPIOTE tasks without CPU intervention.

## Operating Mode

- **PIN1 (LED1)**: GPPI-controlled pulse output
- **PIN2 (LED2)**: Disabled (static LOW, no GPPI)
- **ADC**: Hardware-triggered sampling at pulse end

## Channel Allocation

| Channel Variable | Purpose |
|-----------------|---------|
| `gppi_pin1_set` | Timer → GPIOTE SET (pulse end) |
| `gppi_pin1_clr` | Timer → GPIOTE CLR (pulse start) |
| `gppi_adc_trigger` | Timer → SAADC SAMPLE task |
| `gppi_adc_capture` | SAADC END → Timer CAPTURE |

## Signal Routing

### Pulse Generation (LED1)
```
Timer CC[0] ──GPPI──▶ GPIOTE TASKS_CLR[ch] ──▶ PIN1 LOW (pulse start)
Timer CC[1] ──GPPI──▶ GPIOTE TASKS_SET[ch] ──▶ PIN1 HIGH (pulse end)
```

### ADC Sampling
```
Timer CC[1] ──GPPI──▶ SAADC SAMPLE task
SAADC END  ──GPPI──▶ Timer CAPTURE[4] task (timestamp)
```

## API

### Initialization

```c
nrfx_err_t gppi_init(void);
```

Allocates all required GPPI channels.

**Returns:** NRFX_SUCCESS or error code

### Setup Connections

```c
nrfx_err_t gppi_setup_connections(uint8_t gpiote_ch_pin1, uint8_t gpiote_ch_pin2);
```

Configures GPPI endpoints between timer events and GPIOTE/SAADC tasks.

**Parameters:**
- `gpiote_ch_pin1`: GPIOTE channel for PIN1 (used)
- `gpiote_ch_pin2`: Ignored (PIN2 is static GPIO)

### Enable/Disable

```c
void gppi_enable(void);
void gppi_disable(void);
```

Enables or disables all GPPI channels atomically.

## Hardware Peripherals Connected

| Source | Event/Task | Target |
|--------|-----------|--------|
| STATE_TIMER CC[0] | Compare event | GPIOTE CLR[PIN1] |
| STATE_TIMER CC[1] | Compare event | GPIOTE SET[PIN1] |
| STATE_TIMER CC[1] | Compare event | SAADC SAMPLE |
| SAADC | END event | STATE_TIMER CAPTURE[4] |

## Timing Diagram

```
              CC[0]               CC[1]
Timer:   ──────┴──────────────────┴──────
                ↓                   ↓
PIN1:    ▔▔▔▔▔▔▔▔▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▔▔▔▔▔▔
              (CLR)              (SET)
                                  ↓
ADC:           ─────────────────SAMPLE
```

## Dependencies

- `nrfx_gppi.h`: GPPI helper functions
- `nrfx_timer.h`: Timer event addresses
- `hal/nrf_saadc.h`: SAADC task/event addresses
- `hal/nrf_gpiote.h`: GPIOTE task addresses

## Notes

- PPI/DPPI selection is automatic based on SoC family
- nRF52840 uses PPI (8 channels max per group)
- nRF53/nRF91 series would use DPPI
