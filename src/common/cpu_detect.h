// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.


// Detect the CPU, so we'll know which optimizations to use
#pragma once

#include <string>

namespace Common {

enum CPUVendor
{
    VENDOR_INTEL = 0,
    VENDOR_AMD = 1,
    VENDOR_ARM = 2,
    VENDOR_OTHER = 3,
};

struct CPUInfo
{
    CPUVendor vendor;

    char cpu_string[0x21];
    char brand_string[0x41];
    bool OS64bit;
    bool CPU64bit;
    bool Mode64bit;

    bool HTT;
    int num_cores;
    int logical_cpu_count;

    bool bSSE;
    bool bSSE2;
    bool bSSE3;
    bool bSSSE3;
    bool bPOPCNT;
    bool bSSE4_1;
    bool bSSE4_2;
    bool bLZCNT;
    bool bSSE4A;
    bool bAVX;
    bool bAVX2;
    bool bBMI1;
    bool bBMI2;
    bool bFMA;
    bool bFMA4;
    bool bAES;
    // FXSAVE/FXRSTOR
    bool bFXSR;
    bool bMOVBE;
    // This flag indicates that the hardware supports some mode
    // in which denormal inputs _and_ outputs are automatically set to (signed) zero.
    bool bFlushToZero;
    bool bLAHFSAHF64;
    bool bLongMode;
    bool bAtom;

    // ARMv8 specific
    bool bFP;
    bool bASIMD;
    bool bCRC32;
    bool bSHA1;
    bool bSHA2;

    // Call Detect()
    explicit CPUInfo();

    // Turn the cpu info into a string we can show
    std::string Summarize();

private:
    // Detects the various cpu features
    void Detect();
};

extern CPUInfo cpu_info;

} // namespace Common
