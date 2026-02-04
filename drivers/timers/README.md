# Timer Driver

## Overview

Dual-timer driver for hardware-synchronized pulse generation with MUX pre-loading and per-pulse DAC control.

## Architecture

- **Pulse Timer (TIMER1)**: Generates pulse width signals via GPPI → GPIOTE
- **State Timer (TIMER2)**: Controls 16-pulse state machine with dual CC channels

## Key Features

- **16-Pulse State Machine**: Sequential pulse generation (configurable 1-16 pulses)
- **Dual CC Channel Design**: 
  - CC0: State transition event (main timing)
  - CC1: MUX pre-load event (50µs advance)
- **Per-Pulse MUX Patterns**: Each pulse can activate different channels
- **Per-Pulse DAC Values**: Each pulse can have different amplitude
- **Race-Condition Safe**: IRQ locks protect shared arrays
- **Dynamic Configuration**: Runtime updates via UART commands

## State Machine

```
┌─────────────────────────────────────────────────────────┐
│                    STATE_PULSE                          │
│  current_pulse_idx: 0 → 1 → 2 → ... → (N-1)            │
│  Each pulse: LED1 toggles, MUX pattern applied          │
│  LED2 = HIGH during entire sequence                     │
└───────────────────────────┬─────────────────────────────┘
                            │ current_pulse_idx >= active_pulse_count
                            ▼
┌─────────────────────────────────────────────────────────┐
│                    STATE_PAUSE                          │
│  Duration = period - (active_pulse_count × single_pulse)│
│  LED2 = LOW, MUX = PAUSE pattern                        │
└───────────────────────────┬─────────────────────────────┘
                            │ Pause complete
                            ▼
                   Reset to STATE_PULSE (pulse_idx = 0)
```

## API

### Initialization
```c
nrfx_err_t timer_init(uint32_t pulse_width_us);
```
Initializes both timers with specified pulse width.

### System Control
```c
void timer_system_start(void);  // Start pulse generation
void timer_system_stop(void);   // Stop (IRQ-safe, prevents race condition)
bool timer_system_is_running(void);
```

### MUX Pattern Control
```c
void timer_set_mux_patterns(const uint16_t *patterns, uint8_t count);
uint16_t timer_get_mux_pattern(uint8_t index);
```
Set/get MUX patterns. Count determines active_pulse_count.

### DAC Value Control
```c
void timer_set_dac_values(const uint16_t *values, uint8_t count);
uint16_t timer_get_dac_value(uint8_t index);
```
Set/get DAC values (does NOT affect pulse count).

### Pulse Count
```c
void timer_set_pulse_count(uint8_t count);  // 1-16
uint8_t timer_get_pulse_count(void);
```

### Timing Helpers
```c
uint32_t timer_get_single_pulse_us(void);   // Duration of one pulse
uint32_t timer_get_active_time_us(void);    // Total active time (all pulses)
uint32_t timer_get_max_frequency_hz(void);  // Max achievable frequency
```

### State Control
```c
void timer_set_state_pulse(uint32_t single_pulse_us);
void timer_set_state_pause(uint32_t pause_us);
```

### Monitoring
```c
void timer_get_instances(nrfx_timer_t **p_pulse, nrfx_timer_t **p_state);
uint32_t timer_get_transition_count(void);
```

## Pre-Loading Mechanism

The state timer generates TWO events per pulse:

1. **CC1 Event** (MUX_ADVANCE_TIME_US before transition):
   - Timer ISR calls `mux_write(next_pattern)`
   - Timer ISR calls `dac_set_value(next_value)` if DAC pre-load enabled
   - MUX/DAC are ready BEFORE the pulse starts

2. **CC0 Event** (actual state transition):
   - Advance pulse index or transition to PAUSE
   - Configure next timing period

```
Time: ────────────────────────────────────────────────────►
              │                          │
         CC1 fires                  CC0 fires
      (MUX pre-load)           (state transition)
              │                          │
              ▼                          ▼
         mux_write()              pulse_idx++
         dac_set_value()          configure_next_state()
```

## Race Condition Protection

### timer_system_stop()
Uses IRQ lock to ensure atomic stop:
```c
void timer_system_stop(void) {
    unsigned int key = irq_lock();
    nrfx_timer_disable(&timer_pulse);
    nrfx_timer_disable(&timer_state);
    system_running = false;
    irq_unlock(key);
    nrf_gpio_pin_clear(OUTPUT_PIN_2);  // LED2 LOW
}
```

### timer_set_mux_patterns() / timer_set_dac_values()
Use IRQ lock to prevent ISR reading partially updated arrays:
```c
void timer_set_mux_patterns(...) {
    unsigned int key = irq_lock();
    // ... update mux_patterns[] ...
    irq_unlock(key);
}
```

## Configuration

From `config.h` and `timer_config.h`:

| Parameter | Default | Description |
|-----------|---------|-------------|
| `TIMER_PULSE_IDX` | 1 | Pulse timer instance |
| `TIMER_STATE_IDX` | 2 | State timer instance |
| `MUX_ADVANCE_TIME_US` | 50 | Pre-load advance time |
| `MAX_PULSES_PER_CYCLE` | 16 | Maximum pulse count |
| `NUM_PULSES_PER_CYCLE` | 16 | Default pulse count |
| `PULSE_OVERHEAD_US` | 100 | Per-pulse overhead |

## Dependencies

- `uart.h`: Parameter getters (frequency, pulse width)
- `mux.h`: MUX pattern writes
- `dac.h`: DAC value writes
- `gpiote.h`: Output pin definitions
