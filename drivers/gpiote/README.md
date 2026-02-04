# GPIOTE Driver

## Overview

GPIOTE (GPIO Tasks and Events) driver for hardware-triggered output control. Enables timer-driven LED toggling without CPU intervention using PPI.

## Operating Mode

- **PIN1 (LED1)**: Hardware-controlled via GPIOTE tasks
- **PIN2 (LED2)**: Static GPIO output (permanently LOW)

This configuration allows LED1 to be toggled by timer events through PPI while LED2 remains off.

## Hardware Configuration

| Pin | GPIO | GPIOTE | Function |
|-----|------|--------|----------|
| OUTPUT_PIN_1 | P0.13 (LED1) | Task-enabled | Hardware toggle via PPI |
| OUTPUT_PIN_2 | P0.14 (LED2) | No | Static LOW output |

## API

### Initialization

```c
nrfx_err_t gpiote_init(uint8_t *ch_pin1, uint8_t *ch_pin2);
```

Initializes GPIOTE instance and configures output pins.

**Parameters:**
- `ch_pin1`: Pointer to store allocated GPIOTE channel for PIN1
- `ch_pin2`: Set to 0 (PIN2 uses standard GPIO, no GPIOTE channel)

**Returns:** NRFX_SUCCESS or error code

### Pin Configuration Details

**PIN1 (GPIOTE Task Mode):**
- Task channel: Allocated dynamically
- Polarity: Low-to-High
- Initial value: HIGH (LED off on active-low LEDs)
- Drive: Standard 0, Standard 1 (S0S1)

**PIN2 (Standard GPIO):**
- Configured via `nrf_gpio_cfg_output()`
- Set to LOW permanently via `nrf_gpio_pin_clear()`

## GPIOTE Task Trigger

The GPIOTE task address for PIN1 can be obtained for PPI:

```c
uint32_t task_addr = nrfx_gpiote_out_task_address_get(
    &gpiote_inst, 
    OUTPUT_PIN_1
);
```

## Configuration

In `gpiote_config.h`:

| Parameter | Default | Description |
|-----------|---------|-------------|
| `GPIOTE_INST_IDX` | 0 | GPIOTE peripheral instance |
| `OUTPUT_PIN_1` | LED1_PIN | First output pin (P0.13) |
| `OUTPUT_PIN_2` | LED2_PIN | Second output pin (P0.14) |

## Integration with PPI/GPPI

Timer events trigger GPIOTE tasks through PPI:

```
TIMER CC[0] → PPI → GPIOTE OUT Task → LED1 Toggle
```

See `gppi.c` for PPI channel configuration.

## LED Pin Mapping (nRF52840-DK)

| LED | Pin | Active Level |
|-----|-----|-------------|
| LED1 | P0.13 | Low |
| LED2 | P0.14 | Low |
| LED3 | P0.15 | Low |
| LED4 | P0.16 | Low |

## Dependencies

- `nrfx_gpiote.h`: GPIOTE driver
- `hal/nrf_gpio.h`: Low-level GPIO HAL
- `config.h`: LED pin definitions
