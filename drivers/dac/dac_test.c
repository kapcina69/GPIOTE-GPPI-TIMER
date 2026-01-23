/**
 * @file dac_test.c
 * @brief I2C/TWIM Test Functions
 */

#include "dac.h"
#include "../../config.h"
#include <nrfx_twim.h>
#include <zephyr/kernel.h>

// Get TWIM instance from dac.c
extern nrfx_twim_t twim_inst;

/**
 * @brief Scan I2C bus for devices
 * @details Tries to communicate with all possible I2C addresses (0x03-0x77)
 *          and reports which addresses respond with ACK
 */
void dac_i2c_scan(void)
{
    printk("\n=== I2C Bus Scan ===\n");
    printk("Scanning addresses 0x03-0x77...\n\n");
    
    int found_count = 0;
    uint8_t dummy_data = 0;
    
    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
        nrfx_twim_xfer_desc_t xfer = NRFX_TWIM_XFER_DESC_TX(addr, &dummy_data, 0);
        nrfx_err_t status = nrfx_twim_xfer(&twim_inst, &xfer, 0);
        
        if (status == NRFX_SUCCESS) {
            printk("  [FOUND] Device at address 0x%02X\n", addr);
            found_count++;
        }
        
        // Small delay between scans
        k_msleep(2);
    }
    
    printk("\nScan complete. Found %d device(s).\n", found_count);
    
    if (found_count == 0) {
        printk("\nNo I2C devices found. Possible reasons:\n");
        printk("  - No devices connected to I2C bus\n");
        printk("  - Missing pull-up resistors on SDA/SCL lines\n");
        printk("  - Incorrect pin configuration\n");
        printk("  - Hardware connection issue\n");
    }
    
    printk("===================\n\n");
}

/**
 * @brief Test I2C signal generation (without device)
 * @details Sends a test transaction and checks for bus errors
 */
void dac_i2c_signal_test(void)
{
    printk("\n=== I2C Signal Test ===\n");
    printk("Testing I2C signal generation...\n");
    printk("TWIM instance address: %p\n", (void*)&twim_inst);
    printk("TWIM peripheral: %p\n", (void*)twim_inst.p_twim);
    printk("TWIM drv_inst_idx: %u\n", twim_inst.drv_inst_idx);
    
    uint8_t test_data[2] = {0x40, 0x00};
    nrfx_twim_xfer_desc_t xfer = NRFX_TWIM_XFER_DESC_TX(0x60, test_data, sizeof(test_data));
    
    nrfx_err_t status = nrfx_twim_xfer(&twim_inst, &xfer, 0);
    
    printk("Transfer result: 0x%08X\n", status);
    
    if (status == 0) {
        printk("  Status: SUCCESS (device ACKed)\n");
        printk("  WARNING: No device should be connected!\n");
    } else if (status == 0x0BAD0000 || (status & 0x0BAD0000)) {
        printk("  Status: ADDRESS NACK (no device at 0x60)\n");
        printk("  This is EXPECTED if no device is connected.\n");
        printk("  SCL/SDA signals were generated correctly!\n");
    } else {
        printk("  Status: ERROR/NACK (code 0x%08X)\n", status);
        printk("  Possible meanings:\n");
        printk("    - 0x0BAD0000: NRFX error flag (ANACK/DNACK)\n");
        printk("    - Other: Check nrfx_twim.h error codes\n");
    }
    
    printk("=======================\n\n");
}

/**
 * @brief Check I2C pin states
 */
void dac_i2c_pin_test(void)
{
    printk("\n=== I2C Pin Test ===\n");
    printk("I2C Configuration:\n");
    printk("  SDA Pin: P0.%d\n", DAC_SDA_PIN);
    printk("  SCL Pin: P0.%d\n", DAC_SCL_PIN);
    printk("  I2C Address: 0x%02X\n", DAC_I2C_ADDR);
    printk("\nNote: Use oscilloscope or logic analyzer to verify signals.\n");
    printk("Expected behavior:\n");
    printk("  - SCL should show clock pulses during transfers\n");
    printk("  - SDA should change with data\n");
    printk("  - Both lines idle HIGH (due to pull-ups)\n");
    printk("====================\n\n");
}
