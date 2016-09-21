/*
 * arch/arm/include/asm/vfp.h
 *
 * VFP register definitions.
 * First, the standard VFP set.
 */

#pragma once

// ARM11 MPCore FPSID Information
// Note that these are used as values and not as flags.
enum : u32 {
    VFP_FPSID_IMPLMEN = 0x41, // Implementation code. Should be the same as cp15 0 c0 0
    VFP_FPSID_SW = 0,         // Software emulation bit value
    VFP_FPSID_SUBARCH = 0x1,  // Subarchitecture version number
    VFP_FPSID_PARTNUM = 0x20, // Part number
    VFP_FPSID_VARIANT = 0xB,  // Variant number
    VFP_FPSID_REVISION = 0x4  // Revision number
};

// FPEXC bits
enum : u32 {
    FPEXC_EX = (1U << 31U),
    FPEXC_EN = (1 << 30),
    FPEXC_DEX = (1 << 29),
    FPEXC_FP2V = (1 << 28),
    FPEXC_VV = (1 << 27),
    FPEXC_TFV = (1 << 26),
    FPEXC_LENGTH_BIT = (8),
    FPEXC_LENGTH_MASK = (7 << FPEXC_LENGTH_BIT),
    FPEXC_IDF = (1 << 7),
    FPEXC_IXF = (1 << 4),
    FPEXC_UFF = (1 << 3),
    FPEXC_OFF = (1 << 2),
    FPEXC_DZF = (1 << 1),
    FPEXC_IOF = (1 << 0),
    FPEXC_TRAP_MASK = (FPEXC_IDF | FPEXC_IXF | FPEXC_UFF | FPEXC_OFF | FPEXC_DZF | FPEXC_IOF)
};

// FPSCR Flags
enum : u32 {
    FPSCR_NFLAG = (1U << 31U), // Negative condition flag
    FPSCR_ZFLAG = (1 << 30),   // Zero condition flag
    FPSCR_CFLAG = (1 << 29),   // Carry condition flag
    FPSCR_VFLAG = (1 << 28),   // Overflow condition flag

    FPSCR_QC = (1 << 27),            // Cumulative saturation bit
    FPSCR_AHP = (1 << 26),           // Alternative half-precision control bit
    FPSCR_DEFAULT_NAN = (1 << 25),   // Default NaN mode control bit
    FPSCR_FLUSH_TO_ZERO = (1 << 24), // Flush-to-zero mode control bit
    FPSCR_RMODE_MASK = (3 << 22),    // Rounding Mode bit mask
    FPSCR_STRIDE_MASK = (3 << 20),   // Vector stride bit mask
    FPSCR_LENGTH_MASK = (7 << 16),   // Vector length bit mask

    FPSCR_IDE = (1 << 15), // Input Denormal exception trap enable.
    FPSCR_IXE = (1 << 12), // Inexact exception trap enable
    FPSCR_UFE = (1 << 11), // Undeflow exception trap enable
    FPSCR_OFE = (1 << 10), // Overflow exception trap enable
    FPSCR_DZE = (1 << 9),  // Division by Zero exception trap enable
    FPSCR_IOE = (1 << 8),  // Invalid Operation exception trap enable

    FPSCR_IDC = (1 << 7), // Input Denormal cumulative exception bit
    FPSCR_IXC = (1 << 4), // Inexact cumulative exception bit
    FPSCR_UFC = (1 << 3), // Undeflow cumulative exception bit
    FPSCR_OFC = (1 << 2), // Overflow cumulative exception bit
    FPSCR_DZC = (1 << 1), // Division by Zero cumulative exception bit
    FPSCR_IOC = (1 << 0), // Invalid Operation cumulative exception bit
};

// FPSCR bit offsets
enum : u32 {
    FPSCR_RMODE_BIT = 22,
    FPSCR_STRIDE_BIT = 20,
    FPSCR_LENGTH_BIT = 16,
};

// FPSCR rounding modes
enum : u32 {
    FPSCR_ROUND_NEAREST = (0 << 22),
    FPSCR_ROUND_PLUSINF = (1 << 22),
    FPSCR_ROUND_MINUSINF = (2 << 22),
    FPSCR_ROUND_TOZERO = (3 << 22)
};
