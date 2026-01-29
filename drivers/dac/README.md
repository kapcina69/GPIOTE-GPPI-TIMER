# DAC Driver (MCP4921)

## Overview

This driver targets the MCP4921 SPI DAC (12-bit). The DAC accepts a 16-bit command word where the upper control bits configure the device and the lower 12 bits contain the output value (0..4095). The driver issues non-blocking SPI transfers using nrfx SPIM.

## Hardware
- **Chip**: Microchip MCP4921 (single-channel, 12-bit)
- **Interface**: SPI (NRFX SPIM)
- **Resolution**: 12-bit (0..4095)
- **SPI mode**: Mode 0 (CPOL=0, CPHA=0), MSB-first
- **Typical max SCK**: up to 20 MHz (check DAC datasheet for your VDD)
- **CS**: active low. LDAC pin is optional (can be tied low for immediate update).

## Command / Frame Format

Send a single 16-bit word (MSB first). The typical layout used by this driver is:

- [15]    : Don't care / control bit
- [14]    : BUF (buffered Vref) — usually 0
- [13]    : GA (gain) — 1 = 1x (Vout = Vref * D/4096)
- [12]    : SHDN (shutdown) — 1 = active
- [11:0]  : 12-bit data (D)

Prepare the word in C as:

```c
uint16_t word = (control_bits << 12) | (value & 0x0FFF);
// where control_bits sets BUF/GA/SHDN in the top nibble
```

Example (output active, 1x gain):

```c
uint16_t control = 0b0111; // example: X BUF GA SHDN -> set according to needs
uint16_t value = 2048;     // half-scale
uint16_t tx = (control << 12) | (value & 0x0FFF);
// send tx as big-endian 2 bytes over SPI
```

## Wiring / Pinout (example)

```
nRF52 (devkit)    MCP4921
--------------    -------
P0.26 (MOSI)   ->  DIN / SDI
P0.27 (SCK)    ->  SCK
P0.28 (CS)     ->  CS (active low)
P0.29 (LDAC)   ->  LDAC (optional; tie to GND for immediate update)
GND            ->  GND
VDD            ->  VDD (match DAC Vref)
```

Adjust pins to match your board wiring; the driver uses `drivers/dac/dac_config.h` for pin defines.

## Configuration

Edit `drivers/dac/dac_config.h` (or `config.h`) to match your wiring and SPIM instance. Example:

```c
// NRFX SPIM instance used for the DAC
#define DAC_SPIM_INST_IDX  2

// Pins (nRF pin numbers)
#define DAC_MOSI_PIN       26
#define DAC_SCK_PIN        27
#define DAC_CS_PIN         28
#define DAC_LDAC_PIN       29 // optional

// SPI frequency (Hz)
#define DAC_SPI_FREQUENCY  1000000u // 1 MHz recommended to start
```

Note: If the device-tree contains an SPI node for the same SPIM instance, disable DT auto-init or use matching DT pinctrl to avoid conflicts with manual nrfx init.

## Driver API / Usage

Typical usage in firmware:

```c
// Provide nrfx SPIM instance when initializing
nrfx_spim_t spim = NRFX_SPIM_INSTANCE(DAC_SPIM_INST_IDX);
nrfx_err_t err = dac_init(&spim);

// Set output value (0..4095)
dac_set_value(uint16_t value);

// Internals: the driver packs the 16-bit word into a 2-byte BE buffer
// and performs a non-blocking spim transfer. dac_set_value can be
// used from ISRs if the driver supports ISR-safe enqueueing.
```

Example packing helper:

```c
static inline void dac_send_value(uint16_t value, uint8_t buf[2]) {
	uint16_t control = 0b0111; // set BUF/GA/SHDN as required
	uint16_t tx = (control << 12) | (value & 0x0FFF);
	buf[0] = (uint8_t)(tx >> 8);
	buf[1] = (uint8_t)(tx & 0xFF);
}
```

## Calibration & Vref

- The DAC output range depends on the Vref supplied to the MCP4921. For 1x gain, Vout = Vref * (D/4096).
- If you need a buffered reference, configure the control bits accordingly and follow the datasheet recommendations.

## Notes and Gotchas

- Ensure no device-tree entry auto-initializes the same SPIM instance you use manually; that will cause nrfx init errors (instance already in use).
- CS must be driven active low for transfers. If your board has a dedicated CS control, ensure the driver asserts/de-asserts it correctly or use hardware CS if supported.
- If you use LDAC pin to simultaneously update multiple DACs, wire it and toggle as needed; tying LDAC low updates output immediately.
- SPI clock speed should respect the DAC datasheet limits for your VDD.

## References

- MCP4921 Datasheet (Microchip)
- nrfx SPIM API documentation

