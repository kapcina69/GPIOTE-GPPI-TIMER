# Hardware-Synchronized Pulse Generation System

## Overview

Advanced multi-channel pulse generation system with BLE remote control, hardware-triggered ADC sampling, and SPI-controlled multiplexer. Built on modular driver architecture for nRF52833.

**Key Features:**
- 8-pulse sequential generation with configurable frequency (1-100 Hz)
- Dual-timer architecture with MUX pre-loading for zero-latency switching
- Hardware event routing (GPPI) eliminates CPU intervention
- Real-time BLE parameter control with physical limit validation
- Synchronized ADC sampling on every pulse
- 16-channel multiplexer control via SPI
- DAC amplitude control (MCP4725) via I2C for analog output

## Architecture

### Modular Design
```
main.c (137 lines - orchestration only)
├── services/
│   └── ble - BLE Nordic UART Service with validation
├── drivers/
│   ├── timers/ - Dual-timer state machine
│   ├── gppi/ - Hardware event routing
│   ├── gpiote/ - Task-controlled output pins
│   ├── saadc/ - Hardware-triggered ADC
│   ├── mux/ - SPI multiplexer control
│   └── dac/ - I2C DAC control (MCP4725)
├── config.h - General project configuration
└── drivers/*/.*_config.h - Driver-specific configurations
```

### Hardware Flow
```
Timer State Machine (CC0/CC1) 
    ↓ (GPPI)
GPIOTE Tasks → GPIO Pulses
    ↓ (GPPI)
SAADC Sample → ADC Conversion
    ↓ (GPPI)
MUX Pattern Update (SPI)
```

## Requirements

| **Board**           | **Support** |
|---------------------|:-----------:|
| nrf52833dk_nrf52833 |     Yes     |
| nrf52840dk_nrf52840 |     Yes     |
| nrf5340dk_nrf5340   |   Possible  |

**SDK:** nRF Connect SDK v2.9.1, Zephyr 3.7.99

## Project Structure

### Drivers (`drivers/`)
Each driver is self-contained with README documentation:

- **timers/** - Dual-timer pulse generation
  - `timer.c/h` - 8-pulse state machine with MUX pre-loading
  - Dual CC channels: CC0 (state transition), CC1 (MUX advance)
  
- **gppi/** - Hardware event routing
  - `gppi.c/h` - 6 GPPI channels for zero-latency triggering
  - Connects timer → GPIOTE, timer → SAADC, SAADC → timer
  
- **gpiote/** - GPIO task mode control
  - `gpiote.c/h` - Task-controlled output pins
  - Hardware-triggered pulse generation
  
- **saadc/** - ADC sampling
  - `saadc.c/h` - Continuous sampling with double buffering
  - Hardware-triggered on pulse completion
  
- **mux/** - SPI multiplexer
  - `mux.c/h` - ISR-safe non-blocking SPI control
  - 16-channel analog routing
  
- **dac/** - I2C DAC control
  - `dac.c/h` - MCP4725 12-bit DAC driver
  - Async I2C transfers with race condition protection

### Services (`services/`)
- **ble.c/h** - Nordic UART Service
  - BLE commands: SF; (frequency), SW; (pulse width), SP; (pause)
  - Physical limit validation
  - Auto-adjustment when parameters conflict

### Configuration
**Main Configuration (`config.h`):**
- General pulse generation parameters
- Feature flags (ADC logging, stats timer)
- Logging configuration
- Includes all driver-specific configs

**Driver-Specific Configurations:**
- `drivers/mux/mux_config.h` - SPIM instance, MUX patterns, advance timing
- `drivers/saadc/saadc_config.h` - ADC channels, resolution, batch size
- `drivers/timers/timer_config.h` - Timer instance assignments
- `drivers/gpiote/gpiote_config.h` - GPIOTE instance and output pins
- `drivers/dac/dac_config.h` - I2C instance, pins, DAC address
# Hardware-Synchronized Pulse Generation System

## Overview

Advanced multi-channel pulse generation system with BLE remote control, hardware-triggered ADC sampling, and SPI-controlled multiplexer. Built on modular driver architecture for nRF52833.

**Key Features:**
- 8-pulse sequential generation with configurable frequency (1-100 Hz)
- Dual-timer architecture with MUX pre-loading for zero-latency switching
- Hardware event routing (GPPI) eliminates CPU intervention
- Real-time BLE parameter control with physical limit validation
- Synchronized ADC sampling on every pulse
- 16-channel multiplexer control via SPI
- DAC amplitude control via SPI (external SPI DAC) for analog output

## Architecture

### Modular Design

```
main.c (137 lines - orchestration only)
├── services/
│   └── ble - BLE Nordic UART Service with validation
├── drivers/
│   ├── timers/ - Dual-timer state machine
│   ├── gppi/ - Hardware event routing
│   ├── gpiote/ - Task-controlled output pins
│   ├── saadc/ - Hardware-triggered ADC
│   ├── mux/ - SPI multiplexer control
│   └── dac/ - SPI DAC control (external SPI DAC)
├── config.h - General project configuration
└── drivers/*/*_config.h - Driver-specific configurations
```

### Hardware Flow

```
Timer State Machine (CC0/CC1)
    -> GPPI
GPIOTE Tasks -> GPIO Pulses
    -> GPPI
SAADC Sample -> ADC Conversion
    -> GPPI
MUX Pattern Update (SPI)
```

## Requirements

| **Board**           | **Support** |
|---------------------|:-----------:|
| nrf52833dk_nrf52833 |     Yes     |
| nrf52840dk_nrf52840 |     Yes     |
| nrf5340dk_nrf5340   |   Possible  |

**SDK:** nRF Connect SDK v2.9.1, Zephyr 3.7.99

## Project Structure

### Drivers (`drivers/`)
Each driver is self-contained with README documentation:

- **timers/** - Dual-timer pulse generation
  - `timer.c/h` - 8-pulse state machine with MUX pre-loading
  - Dual CC channels: CC0 (state transition), CC1 (MUX advance)
  
- **gppi/** - Hardware event routing
  - `gppi.c/h` - 6 GPPI channels for zero-latency triggering
  - Connects timer -> GPIOTE, timer -> SAADC, SAADC -> timer
  
- **gpiote/** - GPIO task mode control
  - `gpiote.c/h` - Task-controlled output pins
  - Hardware-triggered pulse generation
  
- **saadc/** - ADC sampling
  - `saadc.c/h` - Continuous sampling with double buffering
  - Hardware-triggered on pulse completion
  
- **mux/** - SPI multiplexer
  - `mux.c/h` - ISR-safe non-blocking SPI control
  - 16-channel analog routing
  
- **dac/** - SPI DAC control
  - `dac.c/h` - SPI DAC driver (generic 12-bit/16-bit compatible)
  - Async SPI transfers with race condition protection

### Services (`services/`)
- **ble.c/h** - Nordic UART Service
  - BLE commands: SF; (frequency), SW; (pulse width), SP; (pause)
  - Physical limit validation
  - Auto-adjustment when parameters conflict

### Configuration
**Main Configuration (`config.h`):**
- General pulse generation parameters
- Feature flags (ADC logging, stats timer)
- Logging configuration
- Includes all driver-specific configs

**Driver-Specific Configurations:**
- `drivers/mux/mux_config.h` - SPIM instance, MUX patterns, advance timing
- `drivers/saadc/saadc_config.h` - ADC channels, resolution, batch size
- `drivers/timers/timer_config.h` - Timer instance assignments
- `drivers/gpiote/gpiote_config.h` - GPIOTE instance and output pins
- `drivers/dac/dac_config.h` - SPIM instance, pins, SPI frequency

## Hardware Connections

### Required Wiring
```
P0.13 (LED1) -> Output Pin 1 (pulse generation)
P0.14 (LED2) -> Output Pin 2 (pulse generation)
P0.1  -> MUX Latch Enable (LE)
P0.0  -> MUX Clear (CLR)

LOOPBACK_PIN_1A -> MOSI (SPI to MUX)
LOOPBACK_PIN_2A -> SCK  (SPI to MUX)

AIN0 (P0.2) -> Analog input for ADC sampling

P0.26 (MOSI) -> DAC MOSI / DIN (SPI)
P0.27 (SCK)  -> DAC SCK (SPI)
P0.28 (CS)   -> DAC CS / LD (SPI) (if required by your DAC)
```

### Optional Connections
- Connect analog multiplexer output to AIN0
- External signal monitoring on LED pins

## Building and Running

### Build
```bash
cd /path/to/non_blocking
west build -b nrf52833dk_nrf52833
```

### Flash
```bash
west flash
```

### Monitor
```bash
# Serial output (115200 baud)
minicom -D /dev/ttyACM0 -b 115200

# Or use nRF Terminal in VS Code
```

## BLE Control

### Connection
1. Open nRF Connect app (Android/iOS)
2. Scan and connect to "nRF52_Pulse_Gen"
3. Enable Nordic UART Service notifications

### Commands
```
SF;25    -> Set frequency to 25 Hz
SW;10    -> Set pulse width to 1000 µs (10 × 100µs)
SP;1     -> Pause pulse generation
SP;0     -> Resume pulse generation
SA;15    -> Set DAC amplitude to 15 (range: 1-30, DAC value = amplitude × 8.5)
```

## Sample Output

See firmware serial logs for startup and status messages (MUX initialized, timers enabled, GPPI configured).

## Configuration Options

### Enable/Disable Features (`config.h`)
```c
#define ENABLE_STATS_TIMER 1     // Periodic statistics output
#define ENABLE_ADC_LOGGING 1     // ADC value printing (in drivers/saadc/saadc_config.h)
#define LOG_EVERY_N_SAMPLES 1024 // ADC logging interval (in drivers/saadc/saadc_config.h)
```

### Adjust Driver-Specific Settings

**MUX Configuration** (`drivers/mux/mux_config.h`):
```c
#define MUX_ADVANCE_TIME_US 50    // MUX pre-load time (10-1000µs)
#define MUX_PATTERN_PULSE_1 0x0101 // Customize for your hardware
```

**SAADC Configuration** (`drivers/saadc/saadc_config.h`):
```c
#define SAADC_RESOLUTION NRF_SAADC_RESOLUTION_10BIT
#define ADC_INTERRUPT_BATCH_SIZE 8
```

**DAC Configuration** (`drivers/dac/dac_config.h`):
```c
// Example SPI-based DAC configuration
#define DAC_SPIM_INST_IDX  2    // NRFX SPIM instance for DAC
#define DAC_MOSI_PIN       26
#define DAC_SCK_PIN        27
#define DAC_CS_PIN         28
#define DAC_SPI_FREQUENCY  1000000 // 1 MHz
```

## Troubleshooting

### No pulses on output pins
- Check GPIOTE initialization in logs
- Verify GPPI connections are enabled
- Ensure timer_pulse_enable() is called

### BLE commands not working
- Check BLE connection status
- Verify command format (SF;25, not SF:25)
- Monitor serial output for error messages

### ADC not sampling
- Verify GPPI connection from timer to SAADC
- Check SAADC initialization logs
- Enable ADC logging to see sample values

### MUX not switching
- Check SPI connections (MOSI, SCK)
- Verify LE pin pulses on scope
- Monitor MUX_ADVANCE_TIME_US (should be 200-500µs)

## Development

### Adding New Driver
1. Create folder in `drivers/` (e.g., `drivers/new_driver/`)
2. Add source files: `new_driver.c`, `new_driver.h`
3. Add to `CMakeLists.txt`:
   ```cmake
   drivers/new_driver/new_driver.c
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/new_driver
   ```
4. Add README.md documenting the driver

### Modifying Parameters
- General configuration: Edit `config.h`
- Driver-specific settings: Edit `drivers/<driver>/<driver>_config.h`
- BLE commands: Edit `services/ble.c`
- State machine: Edit `drivers/timers/timer.c`
- MUX patterns: Edit `drivers/mux/mux_config.h` MUX_PATTERN_* defines
- DAC hardware: Edit `drivers/dac/dac_config.h`

## Documentation

Each module has detailed README:
- [Timer Driver](drivers/timers/README.md) - State machine architecture
- [GPPI Driver](drivers/gppi/README.md) - Hardware event routing
- [GPIOTE Driver](drivers/gpiote/README.md) - Task-controlled outputs
- [SAADC Driver](drivers/saadc/README.md) - ADC sampling details
- [MUX Driver](drivers/mux/README.md) - SPI multiplexer control
- [DAC Driver](drivers/dac/README.md) - SPI DAC control
- [BLE Service](services/README.md) - Command protocol and validation

## License

BSD-3-Clause

## References

- nRF Connect SDK Documentation: https://developer.nordicsemi.com
- nrfx Driver Documentation: https://nrfx.readthedocs.io
- Project Repository: https://github.com/kapcina69/GPIOTE-GPPI-TIMER

