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
#define MUX_ADVANCE_TIME_US 50

/*==============================================================================
 * MUX PATTERNS
 *============================================================================*/

/** 
 * @brief Maximum number of pulses per cycle
 * @note Default is 16 pulses per cycle. Can be reduced via SC command.
 *       In the new mode, LED1 (PIN1) toggles for all pulses while LED2 (PIN2) 
 *       stays low during the entire sequence.
 */
#define MAX_PULSES_PER_CYCLE 16

/** 
 * @brief MUX patterns for 16 sequential pulses
 * @note Each pattern is a 16-bit value sent to the MUX controller.
 *       Each bit corresponds to a MUX channel (bit 0 = channel 1, etc.)
 *       Default: Walking bit pattern - each pulse activates different channel
 *       Can be changed via SC command at runtime.
 */
#define MUX_PATTERN_PULSE_1   0x0001  /* Channel 1 */
#define MUX_PATTERN_PULSE_2   0x0002  /* Channel 2 */
#define MUX_PATTERN_PULSE_3   0x0004  /* Channel 3 */
#define MUX_PATTERN_PULSE_4   0x0008  /* Channel 4 */
#define MUX_PATTERN_PULSE_5   0x0010  /* Channel 5 */
#define MUX_PATTERN_PULSE_6   0x0020  /* Channel 6 */
#define MUX_PATTERN_PULSE_7   0x0040  /* Channel 7 */
#define MUX_PATTERN_PULSE_8   0x0080  /* Channel 8 */
#define MUX_PATTERN_PULSE_9   0x0100  /* Channel 9 */
#define MUX_PATTERN_PULSE_10  0x0200  /* Channel 10 */
#define MUX_PATTERN_PULSE_11  0x0400  /* Channel 11 */
#define MUX_PATTERN_PULSE_12  0x0800  /* Channel 12 */
#define MUX_PATTERN_PULSE_13  0x1000  /* Channel 13 */
#define MUX_PATTERN_PULSE_14  0x2000  /* Channel 14 */
#define MUX_PATTERN_PULSE_15  0x4000  /* Channel 15 */
#define MUX_PATTERN_PULSE_16  0x8000  /* Channel 16 */

/** @brief MUX pattern for PAUSE state (all channels off) */
#define MUX_PATTERN_PAUSE     0x0000

/*==============================================================================
 * VALIDATION
 *============================================================================*/

#if MUX_ADVANCE_TIME_US < 10 || MUX_ADVANCE_TIME_US > 1000
#warning "MUX_ADVANCE_TIME_US is outside recommended range (10-1000µs)"
#endif

#endif /* MUX_CONFIG_H */
