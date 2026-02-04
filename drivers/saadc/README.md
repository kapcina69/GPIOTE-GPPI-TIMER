# SAADC Driver

## Overview

SAADC (Successive Approximation ADC) driver for hardware-triggered continuous sampling. Supports single or dual-channel mode with batched interrupt processing.

## Features

- Hardware-triggered sampling via PPI
- Single or dual-channel operation
- Batched interrupt processing (configurable batch size)
- Automatic buffer management
- Optional periodic logging

## Hardware Configuration

| Parameter | Value |
|-----------|-------|
| Resolution | 10-bit (configurable: 8/10/12/14) |
| Reference | Internal (0.6V) |
| Gain | 1/6 (input range: 0-3.6V) |
| Acquisition time | 10 μs |
| Mode | Single-ended |

### Pin Mapping

| Channel | Default Input | AIN Pin |
|---------|--------------|---------|
| CH0 | AIN0 | P0.02 |
| CH1 | AIN3 (if enabled) | P0.05 |

## API

### Initialization

```c
nrfx_err_t saadc_init(void);
```

Initializes SAADC peripheral, configures channels, and sets up buffer.

### Start Sampling

```c
void saadc_start(void);
```

Begins hardware-triggered continuous sampling.

### Get Latest Sample

```c
int16_t saadc_get_latest_sample_ch0(void);
int16_t saadc_get_latest_sample_ch1(void);  // If dual-channel enabled
```

Returns most recent sample from specified channel.

### Voltage Conversion

```c
int32_t saadc_sample_to_mv(int16_t sample);
```

Converts raw ADC sample to millivolts.

**Formula:**
```
Vout = (sample × 3600) / 1024
```

## Configuration

In `saadc_config.h`:

| Parameter | Default | Description |
|-----------|---------|-------------|
| `SAADC_CHANNEL0_AIN` | AIN0 | First channel input |
| `SAADC_DUAL_CHANNEL_ENABLED` | 0 | Enable second channel |
| `SAADC_CHANNEL1_AIN` | AIN3 | Second channel input |
| `SAADC_RESOLUTION` | 10-bit | ADC resolution |
| `ADC_INTERRUPT_BATCH_SIZE` | 8 | Samples per interrupt |
| `LOG_EVERY_N_SAMPLES` | 100 | Logging frequency |
| `ENABLE_ADC_LOGGING` | 1 | Enable periodic logging |

## Dual-Channel Mode

When enabled, buffer is interleaved:
```
[CH0, CH1, CH0, CH1, CH0, CH1, ...]
```

Enable by setting:
```c
#define SAADC_DUAL_CHANNEL_ENABLED 1
```

## Integration with Timer/GPPI

SAADC sampling is triggered by timer CC[1] event via GPPI:

```
Timer CC[1] ──GPPI──▶ SAADC SAMPLE task
SAADC END   ──GPPI──▶ Timer CAPTURE[4] (timestamp)
```

This synchronizes ADC sampling with pulse generation.

## Buffer Management

```
 Batch Processing
 ├── Buffer: [S0, S1, S2, S3, S4, S5, S6, S7]
 ├── BUF_REQ event → Set new buffer
 ├── DONE event → Process batch
 └── FINISHED event → Restart if needed
```

## Voltage Calculation

With internal reference (0.6V) and gain 1/6:
- Input range: 0 - 3.6V
- 10-bit resolution: 0 - 1023 counts

| ADC Value | Voltage |
|-----------|---------|
| 0 | 0.00 V |
| 512 | 1.80 V |
| 1023 | 3.60 V |

## Dependencies

- `nrfx_saadc.h`: SAADC driver
- `hal/nrf_saadc.h`: Low-level HAL
- `config.h`: Feature flags
