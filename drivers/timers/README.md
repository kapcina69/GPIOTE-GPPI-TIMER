# Timer Driver

## Overview
Dual-timer driver for hardware-synchronized pulse generation with MUX pre-loading.

## Architecture
- **Pulse Timer (TIMER1)**: Generates pulse width signals
- **State Timer (TIMER2)**: Controls 8-pulse state machine with dual CC channels

## Key Features
- **Dual CC Channel Design**: 
  - CC0: State transition event (main timing)
  - CC1: MUX pre-load event (200-500µs advance)
- **8-Pulse State Machine**: Sequential pulse generation with configurable MUX patterns
- **Interrupt-Driven**: Timer events trigger GPPI connections and MUX updates
- **Dynamic Configuration**: Runtime pulse width and frequency adjustment via BLE

## API

### Initialization
```c
nrfx_err_t timer_init(uint32_t pulse_width_us);
```
Initializes both timers with specified pulse width.

### State Control
```c
void timer_set_state_pulse(uint32_t duration_us);
void timer_set_state_pause(uint32_t duration_us);
```
Configure state timer for pulse or pause periods.

### Runtime Updates
```c
void timer_update_pulse_width(uint32_t new_pulse_us);
void timer_pulse_enable(void);
```
Update pulse width and enable pulse generation.

### Monitoring
```c
void timer_get_instances(nrfx_timer_t **p_pulse, nrfx_timer_t **p_state);
uint32_t timer_get_transition_count(void);
```

## State Machine
```
STATE_PULSE_1 → STATE_PULSE_2 → ... → STATE_PULSE_8 → STATE_PAUSE → (repeat)
```

Each state transition:
1. CC1 event fires (MUX advance time before transition)
2. Timer ISR calls `mux_write()` with next pattern
3. CC0 event fires (actual state transition)
4. GPPI triggers pulse generation

## Dependencies
- `config.h`: Timer indices, CC channels, MUX advance time
- `drivers/mux/mux.h`: MUX pattern updates
- `services/ble.h`: BLE parameter updates

## Configuration
See `config.h` for:
- `TIMER_PULSE_IDX`: Pulse timer instance (1)
- `TIMER_STATE_IDX`: State timer instance (2)
- `MUX_ADVANCE_TIME_US`: MUX pre-load timing (200-500µs)
- `MUX_PATTERN_PULSE_*`: 8 pulse patterns
