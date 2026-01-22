# BLE Service

## Overview
BLE Nordic UART Service (NUS) implementation for wireless remote control of pulse generation parameters.

## Purpose
Provides wireless interface for real-time configuration of:
- Pulse frequency (1-100 Hz)
- Pulse width (100-10,000 µs)
- System control (start/pause)

## Features
- **Frequency Validation**: Prevents physically impossible frequency/pulse width combinations
- **Auto-Adjustment**: Automatically reduces frequency if pulse width change makes current frequency unachievable
- **Real-Time Updates**: Immediate parameter changes without system restart
- **Bidirectional Communication**: Commands and status responses

## Command Protocol

### Set Frequency (SF;)
```
SF;25
```
Sets pulse frequency to 25 Hz.

**Validation:**
- Checks if frequency is achievable with current pulse width
- Formula: `max_freq = 1,000,000 / [(pulse_width×100)×2+100]×8`
- Rejects invalid frequencies with detailed error message

**Example:**
```
// Current pulse width: 1000µs (10 in BLE units)
SF;100   → ERROR: "Invalid frequency! Max achievable: 59 Hz"
SF;50    → OK: Frequency set to 50 Hz
```

### Set Pulse Width (SW;)
```
SW;5
```
Sets pulse width to 500 µs (5 × 100 µs).

**Auto-Adjustment:**
- If new pulse width reduces max achievable frequency below current setting
- Automatically adjusts frequency to new maximum
- Notifies user of change

**Example:**
```
// Current: 80 Hz, pulse width 5 (500µs)
SW;15    → "Pulse width set to 1500 us. Frequency auto-adjusted to 39 Hz"
```

### Pause/Resume (SP;)
```
SP;1    // Pause pulse generation
SP;0    // Resume pulse generation
```

## Validation Logic

### Physical Limits
The system enforces timing constraints based on 8-pulse cycle:

```
Active Time = [(pulse_width × 100) × 2 + 100] × 8 µs
Max Frequency = 1,000,000 µs / Active Time
```

**Example Calculations:**
| Pulse Width | Active Time | Max Frequency |
|-------------|-------------|---------------|
| 1 (100µs)   | 3,200 µs    | 312 Hz        |
| 5 (500µs)   | 8,800 µs    | 113 Hz        |
| 10 (1000µs) | 16,800 µs   | 59 Hz         |
| 50 (5000µs) | 80,800 µs   | 12 Hz         |

### Error Messages
```
"Invalid frequency! Max achievable with 1000us pulse: 59 Hz"
"Pulse width must be 1-100 (100us-10000us)"
"Unknown command"
```

## API

### Initialization
```c
int ble_init(void);
```
Initializes BLE stack and starts advertising.

### Parameter Access
```c
uint32_t ble_get_pulse_width_ms(void);
uint32_t ble_get_frequency_hz(void);
```
Get current pulse width (in 100µs units) and frequency.

### Notifications
```c
void ble_notify_status(const char *message);
```
Send status message to connected device (internal use).

## Connection Flow
```
1. Power on → Auto advertising starts
2. Connect via BLE terminal app (nRF Connect, Serial Bluetooth Terminal)
3. Discover Nordic UART Service (6E400001-B5A3-F393-E0A9-E50E24DCCA9E)
4. Enable RX characteristic notifications
5. Send commands via TX characteristic
```

## Integration with Hardware Drivers

### Frequency Change
```
BLE receives SF; command
  → Validates with physical limits
  → Calls timer_set_state_pulse() with new period
  → System adjusts without stopping
```

### Pulse Width Change
```
BLE receives SW; command
  → Calculates new max frequency
  → Auto-adjusts frequency if needed
  → Calls timer_update_pulse_width()
  → Notifies user of changes
```

## Dependencies
- `config.h`: `CALCULATE_ACTIVE_TIME_US`, `CALCULATE_MAX_FREQUENCY_HZ` macros
- `drivers/timers/timer.h`: Parameter update functions
- Zephyr BLE stack: NUS service implementation

## Configuration
BLE parameters in `prj.conf`:
```
CONFIG_BT=y
CONFIG_BT_PERIPHERAL=y
CONFIG_BT_DEVICE_NAME="nRF52_Pulse_Gen"
CONFIG_BT_NUS=y
```

## Usage Example
```
// Connect via nRF Connect app
// Send commands:

SF;10     → "Frequency set to 10 Hz"
SW;20     → "Pulse width set to 2000 us"
SF;100    → "Invalid frequency! Max achievable: 49 Hz"
SW;50     → "Pulse width set to 5000 us. Frequency adjusted to 12 Hz"
SP;1      → "Pulse generation paused"
SP;0      → "Pulse generation resumed"
```

## Error Handling
- Invalid format → "Unknown command"
- Out of range → "Pulse width must be 1-100"
- Physically impossible → Shows max achievable frequency with current pulse width
- Connection lost → System continues with last parameters

## Performance
- **Latency**: ~50ms from command to hardware update
- **Update Rate**: Can handle commands as fast as BLE throughput allows
- **Validation Overhead**: Minimal (<1ms calculation time)
