# MUX Driver

## Overview
Multiplexer driver for SPI-based 16-channel control using shift registers.

## Purpose
Controls external 16-channel multiplexer via SPI to route analog signals to ADC input, synchronized with pulse generation state machine.

## Hardware Interface
- **SPI Peripheral**: SPIM2 (non-blocking async transfers)
- **Shift Register**: 2-byte (16-bit) pattern sent MSB first
- **Control Pins**:
  - `MUX_LE_PIN` (P0.1): Latch Enable - pulses HIGH to latch data
  - `MUX_CLR_PIN` (P0.0): Clear - clears all channels when HIGH

## ISR-Safe Design
Optimized for calling from interrupt context:
- **Non-blocking**: Returns immediately, transfer completes in background
- **Status Check**: `mux_is_ready()` polls transfer completion
- **Busy Handling**: Returns `NRFX_ERROR_BUSY` if previous transfer ongoing

## API

### Initialization
```c
nrfx_err_t mux_init(nrfx_spim_t *spim);
```
Initializes SPIM, configures GPIO control pins, clears all channels.

### Write Operations
```c
nrfx_err_t mux_write(uint16_t data);
```
Sends 16-bit pattern to MUX (non-blocking). Each bit position corresponds to a channel.

**Example Patterns:**
```c
0x0101  // Channel 1 + Channel 9
0x0202  // Channel 2 + Channel 10
0x8080  // Channel 8 + Channel 16
```

### Status Check
```c
bool mux_is_ready(void);
void mux_wait_ready(void);
```
Check or wait for transfer completion.

## Transfer Sequence
1. `mux_write()` called with 16-bit pattern
2. SPI transfer initiated (MSB first)
3. Function returns immediately (non-blocking)
4. SPI interrupt fires on completion
5. LE pin pulsed to latch data into output registers

## Integration with State Machine
```c
// Timer ISR calls mux_write() on CC1 event (MUX pre-load)
void state_timer_handler(nrf_timer_event_t event, void *context) {
    if (event == NRF_TIMER_EVENT_COMPARE1) {
        mux_write(next_pattern);  // Non-blocking, safe in ISR
    }
}
```

## Timing Considerations
- **Pre-load Advance**: MUX pattern sent 200-500µs before pulse (configurable via `MUX_ADVANCE_TIME_US`)
- **Latch Pulse Width**: ~50-100ns (4 NOP instructions @ 64MHz)
- **SPI Clock**: Configured via `NRFX_SPIM_DEFAULT_CONFIG`

## Dependencies
- `config.h`: SPIM instance index, control pin definitions
- `nrfx_spim.h`: SPIM driver functions
- `hal/nrf_gpio.h`: GPIO control for LE and CLR pins

## Error Handling
- Returns `NRFX_ERROR_BUSY` if transfer already in progress
- Check `mux_is_ready()` before calling `mux_write()` in critical paths
- Use `mux_wait_ready()` only outside ISR context (blocking)

## Hardware Connection Example
```
MCU SPIM2 → 74HC595/74HC164 Shift Register → 16-channel Analog MUX
            LE Pin → Latch Enable
            CLR Pin → Clear
```

## Performance
- **Transfer Time**: ~16µs for 16-bit @ 1MHz SPI
- **CPU Overhead**: Minimal (async transfer + ISR callback)
- **Safe for ISR**: Yes (non-blocking design)
