# DAC Driver (MCP4725)

## Overview
I2C-based 12-bit DAC driver using nrfx_twim API (no device tree overlay required).

## Hardware
- **Chip**: MCP4725 
- **Interface**: I2C (TWIM)
- **Resolution**: 10-bit (0-1023)
- **Default Address**: 0x60

## Configuration
Configure in `config.h`:
```c
#define DAC_TWIM_INST_IDX 0      // TWIM instance (0 or 1)
#define DAC_SDA_PIN       26     // I2C data pin
#define DAC_SCL_PIN       27     // I2C clock pin
#define DAC_I2C_ADDR      0x60   // MCP4725 I2C address
```

## Usage
```c
// Initialize
nrfx_err_t status = dac_init();

// Set output (0-1023)
dac_set_value(512);  // Mid-scale
dac_set_value(0);    // 0V
dac_set_value(1023); // Vref
```

## Pin Connection
```
nRF52833          MCP4725
--------          -------
P0.26 (SDA) ----> SDA
P0.27 (SCL) ----> SCL
GND ------------> GND
VDD ------------> VCC
```
