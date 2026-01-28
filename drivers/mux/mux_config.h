/**
 * @file mux_config.h
 * @brief MUX Driver Configuration
 * 
 * Configuration parameters specific to the MUX driver.
 */

#ifndef MUX_CONFIG_H
#define MUX_CONFIG_H

/*==============================================================================
 * MUX HARDWARE CONFIGURATION
 *============================================================================*/

/** @brief SPIM instance index for MUX control */
#define SPIM_INST_IDX 1

/** 
 * @brief MUX pre-load advance time in microseconds
 * @note The state timer generates TWO events per state:
 *       - CC1 fires ADVANCE_TIME microseconds BEFORE state transition
 *       - CC0 fires at the actual state transition time
 *       
 *       This ensures the MUX pattern is sent early enough to arrive
 *       via SPI before the pulse starts.
 *       
 *       Typical values:
 *       - 50µs: Fast SPI, short cables
 *       - 200µs: Normal operation (recommended)
 *       - 500µs: Slow SPI or long cables
 */
#define MUX_ADVANCE_TIME_US 35

/*==============================================================================
 * MUX PATTERNS
 *============================================================================*/

/** 
 * @brief MUX patterns for 8 sequential pulses
 * @note Each pattern is a 16-bit value sent to the MUX controller.
 *       Pattern format depends on your MUX hardware design.
 *       Default patterns use a walking bit pattern for demonstration.
 *       
 *       Customize these patterns based on your MUX routing requirements:
 *       - Pulse 1: Channel 0 (0x0101)
 *       - Pulse 2: Channel 1 (0x0202)
 *       - Pulse 3: Channel 2 (0x0404)
 *       - Pulse 4: Channel 3 (0x0808)
 *       - Pulse 5: Channel 4 (0x1010)
 *       - Pulse 6: Channel 5 (0x2020)
 *       - Pulse 7: Channel 6 (0x4040)
 *       - Pulse 8: Channel 7 (0x8080)
 */
#define MUX_PATTERN_PULSE_1  0x0101
#define MUX_PATTERN_PULSE_2  0x0202
#define MUX_PATTERN_PULSE_3  0x0404
#define MUX_PATTERN_PULSE_4  0x0808
#define MUX_PATTERN_PULSE_5  0x1010
#define MUX_PATTERN_PULSE_6  0x2020
#define MUX_PATTERN_PULSE_7  0x4040
#define MUX_PATTERN_PULSE_8  0x8080

/** @brief MUX pattern for PAUSE state (all channels off) */
#define MUX_PATTERN_PAUSE    0x0000

/*==============================================================================
 * VALIDATION
 *============================================================================*/

#if MUX_ADVANCE_TIME_US < 10 || MUX_ADVANCE_TIME_US > 1000
#warning "MUX_ADVANCE_TIME_US is outside recommended range (10-1000µs)"
#endif

#endif /* MUX_CONFIG_H */
