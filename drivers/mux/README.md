# MUX Driver

## Overview

Non-blocking SPI multiplexer driver for 16-channel analog signal routing.

## Purpose

Controls external 16-channel multiplexer via SPI shift registers. Each bit in the 16-bit pattern corresponds to one channel. The MUX pattern is pre-loaded before each pulse to ensure zero-latency channel switching.

## Hardware Interface

| Pin | Function | Description |
|-----|----------|-------------|
| P0.03 | MOSI | SPI data out |
| P0.05 | SCK | SPI clock |
| P0.01 | LE | Latch Enable (pulses HIGH to latch) |
| P0.00 | CLR | Clear (not used in normal operation) |

**SPI Instance:** SPIM1

## Pattern Format

16-bit pattern, MSB first. Each bit enables one channel:

```
Bit:   15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
Chan:  16  15  14  13  12  11  10   9   8   7   6   5   4   3   2   1
```

### Example Patterns

| Pattern | Channels Active |
|---------|-----------------|
| `0x0001` | Channel 1 only |
| `0x0002` | Channel 2 only |
| `0x8000` | Channel 16 only |
| `0x00FF` | Channels 1-8 |
| `0xFF00` | Channels 9-16 |
| `0x5555` | Odd channels (1,3,5...) |
| `0x0000` | All off (pause pattern) |

## ISR-Safe Design

Optimized for calling from timer interrupt context:

- **Non-blocking**: `mux_write()` returns immediately
- **Async completion**: SPI interrupt handles transfer end
- **Busy handling**: Returns `NRFX_ERROR_BUSY` if transfer ongoing
- **Status polling**: `mux_is_ready()` checks completion

## API

### Initialization
```c
nrfx_err_t mux_init(nrfx_spim_t *spim);
```
Initializes SPIM, configures LE/CLR pins, clears all channels.

### Write Pattern
```c
nrfx_err_t mux_write(uint16_t data);
```
Sends 16-bit pattern (non-blocking). Returns immediately.

### Status
```c
bool mux_is_ready(void);      // Check if ready for new transfer
void mux_wait_ready(void);    // Busy-wait for completion
```

## Transfer Sequence

```
1. mux_write(pattern) called
2. Convert to big-endian bytes
3. Start SPI transfer (non-blocking)
4. Function returns immediately
5. SPI interrupt fires on completion
6. LE pin pulsed HIGH→LOW to latch data
7. m_xfer_done = true
```

## Integration with Timer

The timer ISR calls `mux_write()` on CC1 event (pre-load):

```c
// In state_timer_handler()
if (event_type == NRF_TIMER_EVENT_COMPARE1) {
    // Pre-load next MUX pattern
    mux_write(mux_patterns[next_idx]);
}
```

This ensures the MUX pattern arrives ~50µs BEFORE the pulse starts.

## Default Patterns (Walking Bit)

Defined in `mux_config.h`:

```c
#define MUX_PATTERN_PULSE_1   0x0001  // Channel 1
#define MUX_PATTERN_PULSE_2   0x0002  // Channel 2
#define MUX_PATTERN_PULSE_3   0x0004  // Channel 3
...
#define MUX_PATTERN_PULSE_16  0x8000  // Channel 16
#define MUX_PATTERN_PAUSE     0x0000  // All off
```

## Configuration

In `mux_config.h`:

| Parameter | Value | Description |
|-----------|-------|-------------|
| `SPIM_INST_IDX` | 1 | SPI instance for MUX |
| `MUX_ADVANCE_TIME_US` | 50 | Pre-load advance time |
| `MUX_LE_PIN` | 1 | Latch Enable pin |
| `MUX_CLR_PIN` | 0 | Clear pin |

## Dependencies

- `nrfx_spim.h`: SPI master driver
- `config.h`: Pin definitions
