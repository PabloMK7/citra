// Copyright 2013 Dolphin Emulator Project / 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

namespace Common {

/// x86/x64 CPU vendors that may be detected by this module
enum class CPUVendor {
    INTEL,
    AMD,
    OTHER,
};

/// x86/x64 CPU capabilities that may be detected by this module
struct CPUCaps {
    CPUVendor vendor;
    char cpu_string[0x21];
    char brand_string[0x41];
    int num_cores;
    bool sse;
    bool sse2;
    bool sse3;
    bool ssse3;
    bool sse4_1;
    bool sse4_2;
    bool lzcnt;
    bool avx;
    bool avx2;
    bool bmi1;
    bool bmi2;
    bool fma;
    bool fma4;
    bool aes;

    // Support for the FXSAVE and FXRSTOR instructions
    bool fxsave_fxrstor;

    bool movbe;

    // This flag indicates that the hardware supports some mode in which denormal inputs and outputs
    // are automatically set to (signed) zero.
    bool flush_to_zero;

    // Support for LAHF and SAHF instructions in 64-bit mode
    bool lahf_sahf_64;

    bool long_mode;
};

/**
 * Gets the supported capabilities of the host CPU
 * @return Reference to a CPUCaps struct with the detected host CPU capabilities
 */
const CPUCaps& GetCPUCaps();

/**
 * Gets a string summary of the name and supported capabilities of the host CPU
 * @return String summary
 */
std::string GetCPUCapsString();

} // namespace Common
