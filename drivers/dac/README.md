# DAC Driver (MCP4921)

## Overview

Non-blocking SPI DAC driver for MCP4921 12-bit digital-to-analog converter. Provides per-pulse amplitude control for the stimulation system.

## Hardware

| Parameter | Value |
|-----------|-------|
| Chip | Microchip MCP4921 |
| Resolution | 12-bit (0-4095) |
| Interface | SPI (SPIM2) |
| SPI Mode | Mode 0 (CPOL=0, CPHA=0) |
| Max SCK | 20 MHz |
| Output | Single-channel |

## Pin Configuration

| Pin | Function |
|-----|----------|
| P0.26 | MOSI (DIN) |
| P0.27 | SCK |
| P0.28 | CS (active low) |
| - | LDAC (tie to GND for immediate update) |

## Command Format

16-bit SPI word (MSB first):

```
Bit: 15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
     X   BUF GA  SHDN ─────────── DATA (12-bit) ───────────────────
```

| Bit | Name | Function |
|-----|------|----------|
| 15 | X | Don't care |
| 14 | BUF | Buffered Vref (0 = unbuffered) |
| 13 | GA | Gain (1 = 1×, 0 = 2×) |
| 12 | SHDN | Shutdown (1 = active) |
| 11:0 | DATA | 12-bit output value |

### Typical Configuration

```c
// Active, 1× gain, unbuffered: control = 0b0111 = 0x7
uint16_t word = (0x7 << 12) | (value & 0x0FFF);
```

## API

### Initialization
```c
nrfx_err_t dac_init(nrfx_spim_t *spim);
```
Initializes SPIM2 for DAC communication, configures CS pin.

### Set Value
```c
nrfx_err_t dac_set_value(uint16_t value);
```
Sets DAC output (0-4095). Non-blocking SPI transfer.

**Parameters:**
- `value`: 12-bit DAC value (0x000 - 0xFFF)

**Returns:** NRFX_SUCCESS or NRFX_ERROR_BUSY

### Status
```c
bool dac_is_ready(void);
void dac_wait_ready(void);
```

## Output Voltage

With 1× gain and Vref = VDD:

```
Vout = Vref × (value / 4096)
```

| Value | Output (3.3V Vref) |
|-------|-------------------|
| 0 | 0.00 V |
| 2048 | 1.65 V |
| 4095 | 3.30 V |

## Integration with Timer

DAC values are pre-loaded with MUX patterns:

```c
// In state_timer_handler() CC1 event
if (ENABLE_DAC_PRELOAD) {
    dac_set_value(dac_values[next_idx]);
}
```

## Per-Pulse DAC Values

Set via SA command or `timer_set_dac_values()`:

```c
// Default: Linear ramp 200 → 4000
static uint16_t dac_values[16] = {
    200,   // Pulse 1
    450,   // Pulse 2
    ...
    4000   // Pulse 16
};
```

### UART Command
```
>SA;00C8 01C2 02BC 03B6<
```
Sets DAC values for 4 pulses (0x00C8=200, 0x01C2=450, etc.)

## Configuration

In `dac_config.h`:

| Parameter | Value | Description |
|-----------|-------|-------------|
| `DAC_SPIM_INST_IDX` | 2 | SPI instance for DAC |
| `DAC_MOSI_PIN` | 26 | MOSI pin |
| `DAC_SCK_PIN` | 27 | SCK pin |
| `DAC_CS_PIN` | 28 | CS pin |
| `ENABLE_DAC_PRELOAD` | 1 | Enable per-pulse DAC |

## Dependencies

- `nrfx_spim.h`: SPI master driver
- `config.h`: Feature flags, pin definitions
