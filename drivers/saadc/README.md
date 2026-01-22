# SAADC Driver

## Overview
SAADC (Successive Approximation ADC) driver for hardware-triggered continuous analog sampling.

## Purpose
Configures SAADC for automatic sampling synchronized with pulse generation, enabling real-time voltage monitoring without CPU intervention.

## Features
- **Hardware-Triggered Sampling**: GPPI triggers ADC on pulse completion
- **Double Buffering**: Continuous sampling with automatic buffer swap
- **Batch Processing**: Processes multiple samples per interrupt
- **Continuous Mode**: Automatic restart after conversion

## Configuration
- **Mode**: Single-ended
- **Reference**: Internal (0.6V)
- **Gain**: 1/6 (3.6V range)
- **Resolution**: 12-bit (configurable via `SAADC_RESOLUTION`)
- **Acquisition Time**: 10µs
- **Batch Size**: Configurable via `ADC_INTERRUPT_BATCH_SIZE`

## API

### Initialization
```c
nrfx_err_t saadc_init(void);
```
Initializes SAADC with double-buffering and continuous mode.

### Data Access
```c
int16_t saadc_get_latest_sample(void);
uint32_t saadc_get_sample_count(void);
int32_t saadc_sample_to_mv(int16_t sample);
```
Access latest sample value, total count, and voltage conversion.

## Interrupt Flow
```
1. SAADC_EVT_BUF_REQ   → Provide next buffer
2. SAADC_EVT_DONE      → Process completed samples
3. SAADC_EVT_FINISHED  → Set new buffer and trigger next conversion
```

## Hardware Integration
```
TIMER.CC1 → GPPI → SAADC.SAMPLE task → ADC conversion
SAADC.END event → GPPI → TIMER.CAPTURE4 → Timestamp capture
```

## Voltage Calculation
```c
voltage_mV = (sample * 3600) / 1024
```
With 1/6 gain and 0.6V reference:
- Full scale = 3.6V
- Resolution = ~3.5 mV/LSB (12-bit)

## Logging
Controlled by `ENABLE_ADC_LOGGING` in `config.h`:
- Prints every N samples (configurable via `LOG_EVERY_N_SAMPLES`)
- Format: `[ADC] #count: voltage mV`

## Dependencies
- `config.h`: ADC configuration and channel settings
- `nrfx_saadc.h`: SAADC driver functions
- `hal/nrf_saadc.h`: SAADC HAL definitions

## Performance
- **Sampling Rate**: Determined by pulse frequency
- **Latency**: ~10µs acquisition + conversion time
- **CPU Load**: Minimal (interrupt-driven with batching)

## Usage Notes
- Channel must be connected to analog input pin (configurable in `config.h`)
- IRQ handler automatically registered during initialization
- Buffer management handled automatically via double-buffering
- Stats available via `saadc_get_sample_count()` for throughput monitoring
