# GPPI Driver

## Overview
GPPI (Generic PPI) driver manages hardware event routing between peripherals without CPU intervention.

## Purpose
Connects timer events to GPIOTE tasks and SAADC tasks for zero-latency pulse generation and sampling.

## Channel Allocation
Allocates 6 GPPI channels:
- **2 channels for PIN1**: Clear and Set tasks
- **2 channels for PIN2**: Clear and Set tasks  
- **2 channels for ADC**: Sample trigger and capture

## Connections

### Pulse Generation
```
TIMER1.CC0 → GPIOTE.PIN1_CLR  (pulse start - clear pin)
TIMER1.CC1 → GPIOTE.PIN1_SET  (pulse end - set pin)
TIMER1.CC2 → GPIOTE.PIN2_CLR  (pulse start - clear pin)
TIMER1.CC3 → GPIOTE.PIN2_SET  (pulse end - set pin)
```

### ADC Sampling
```
TIMER1.CC1 → SAADC.SAMPLE      (trigger ADC on pulse end)
SAADC.END  → TIMER2.CAPTURE4   (timestamp ADC completion)
```

## API

### Initialization
```c
nrfx_err_t gppi_init(void);
```
Allocates 6 GPPI channels.

### Setup
```c
nrfx_err_t gppi_setup_connections(uint8_t gpiote_ch_pin1, uint8_t gpiote_ch_pin2);
```
Configures all hardware event-to-task connections.

### Enable
```c
void gppi_enable(void);
```
Activates all GPPI channels simultaneously.

## Benefits
- **Zero CPU Overhead**: Hardware-to-hardware event routing
- **Precise Timing**: No interrupt latency
- **Power Efficient**: CPU can sleep during pulse generation
- **Synchronized Sampling**: ADC samples at exact pulse end

## Dependencies
- `config.h`: Hardware indices and pin definitions
- `drivers/timers/timer.h`: Timer instance access
- `nrfx_gppi.h`: GPPI HAL functions
- `hal/nrf_saadc.h`: SAADC task addresses

## Hardware Requirements
- DPPI peripheral (nRF52/nRF53 series)
- Timer with multiple CC channels
- GPIOTE configured for task mode
- SAADC peripheral
