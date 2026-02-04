# UART Driver

## Overview

Non-blocking UART command interface using nrfx UARTE driver with work queue processing for ISR-safe operation.

## Key Features

- **Non-Blocking RX**: Commands processed via Zephyr work queue, not in ISR
- **Race-Safe TX**: Atomic test-and-set for TX busy flag
- **Command Protocol**: `>CMD;args<` format with validation
- **Loopback Testing**: Built-in timer sends test commands to self
- **Parameter Management**: Thread-safe frequency/pulse width updates

## Hardware Configuration

| Parameter | Value |
|-----------|-------|
| Instance | UARTE1 |
| TX Pin | P0.06 |
| RX Pin | P0.08 |
| Baudrate | 115200 |
| RX Chunk | 1 byte (character-by-character) |

## Command Protocol

### Format
```
>CMD;arguments<
```
- `>` = Start marker
- `CMD` = Command code (2-4 chars)
- `;` = Argument separator (optional)
- `arguments` = Hex values, space-separated for multiple
- `<` = End marker

### Supported Commands

| Command | Format | Description | Response |
|---------|--------|-------------|----------|
| SON | `>SON<` | Start stimulation | `>SONOK<` |
| SOFF | `>SOFF<` | Stop stimulation | `>OFFOK<` |
| PW | `>PW;X<` | Pulse width (hex 1-A) | `>PWOK<` or `>PWERR<` |
| SF | `>SF;XX<` | Frequency Hz (hex 1-64) | `>SFOK<` or `>SFERR<` |
| SC | `>SC;XXXX ...<` | MUX patterns (up to 16) | `>SCOK<` |
| SA | `>SA;XXXX ...<` | DAC values (up to 16) | `>SAOK<` |

### Examples

```
>SON<              # Start
>PW;5<             # Pulse width = 5 (500µs)
>SF;19<            # Frequency = 25 Hz (0x19)
>SC;0001 0002 0004 0008<   # 4 MUX patterns (4 pulses)
>SA;0200 0400 0600 0800<   # 4 DAC values
>SOFF<             # Stop
```

## API

### Initialization
```c
int uart_init(void);
```
Initializes UARTE1, registers handler, starts RX.

### Sending
```c
int uart_send(const char *data);
void uart_send_response(const char *response);
```
Send raw data or formatted response (`>response<`).

### TX Status
```c
bool uart_is_tx_busy(void);
```

### Test Timer
```c
void uart_start_test_timer(uint32_t interval_ms);
void uart_stop_test_timer(void);
```
Starts/stops loopback test timer.

### Parameter Access
```c
uint32_t uart_get_frequency_hz(void);
uint32_t uart_get_pulse_width_ms(void);
uint32_t uart_get_max_frequency(uint32_t pulse_width);
uint32_t uart_get_pause_time_ms(void);
```

### Update Flag (Race-Safe)
```c
bool uart_parameters_updated(void);
void uart_clear_update_flag(void);
bool uart_test_and_clear_update_flag(void);  // Atomic test-and-clear
```

## Non-Blocking Architecture

### Problem Solved
Direct command processing in UART ISR blocks timer ISR, causing pulse timing issues.

### Solution: Work Queue

```
UART RX Interrupt (ISR)
    ↓ (~2µs)
strncpy(pending_cmd, cmd_buffer)
k_work_submit(&cmd_work)
    ↓
ISR RETURNS IMMEDIATELY
    
    ... later (when scheduler runs) ...
    
System Workqueue Thread
    ↓
cmd_work_handler()
    ↓
process_command()  ← printk() safe here
```

### Benefits
- Timer ISR never blocked by UART processing
- Pulse timing remains precise
- printk()/LOG calls don't affect timing

## Race Condition Protection

### TX Busy Flag (Atomic)
```c
static atomic_t tx_busy = ATOMIC_INIT(0);

// Acquire TX lock atomically
if (!atomic_cas(&tx_busy, 0, 1)) {
    return;  // TX busy, skip
}
// ... do TX ...
// Release on TX_DONE event
atomic_set(&tx_busy, 0);
```

### Parameters Updated Flag (Atomic)
```c
static atomic_t parameters_updated_flag = ATOMIC_INIT(0);

// Set (from UART handler)
atomic_set(&parameters_updated_flag, 1);

// Test-and-clear (from Timer ISR) - atomic
bool uart_test_and_clear_update_flag(void) {
    return atomic_cas(&parameters_updated_flag, 1, 0);
}
```

## Loopback Test Mode

Built-in stress test sends commands to self (TX → RX loopback):

```c
// In main.c
uart_start_test_timer(600);  // Send command every 600ms
```

Test sequence includes:
- SON/SOFF cycles
- Pulse width sweep (1-10)
- Frequency sweep (1-100 Hz)
- MUX pattern variations
- DAC value ramps

## Configuration

In `uart.h`:
```c
#define UARTE_INST_IDX      1
#define UARTE_TX_PIN        6
#define UARTE_RX_PIN        8
#define CMD_BUFFER_SIZE     128
#define TX_BUFFER_SIZE      128
#define MIN_FREQUENCY_HZ    1
#define MAX_FREQUENCY_HZ    100
#define MIN_PULSE_WIDTH     1
#define MAX_PULSE_WIDTH     10
```

## Dependencies

- `timer.h`: `timer_set_mux_patterns()`, `timer_set_dac_values()`, etc.
- `<zephyr/sys/atomic.h>`: Atomic operations
- `<zephyr/kernel.h>`: Work queue (`K_WORK_DEFINE`)
