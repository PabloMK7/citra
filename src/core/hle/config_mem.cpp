// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/log.h"

#include "core/hle/config_mem.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace ConfigMem {

enum {
    KERNEL_VERSIONREVISION  = 0x1FF80001,
    KERNEL_VERSIONMINOR     = 0x1FF80002,
    KERNEL_VERSIONMAJOR     = 0x1FF80003,
    UPDATEFLAG              = 0x1FF80004,
    NSTID                   = 0x1FF80008,
    SYSCOREVER              = 0x1FF80010,
    UNITINFO                = 0x1FF80014,
    KERNEL_CTRSDKVERSION    = 0x1FF80018,
    APPMEMTYPE              = 0x1FF80030,
    APPMEMALLOC             = 0x1FF80040,
    FIRM_VERSIONREVISION    = 0x1FF80061,
    FIRM_VERSIONMINOR       = 0x1FF80062,
    FIRM_VERSIONMAJOR       = 0x1FF80063,
    FIRM_SYSCOREVER         = 0x1FF80064,
    FIRM_CTRSDKVERSION      = 0x1FF80068,
};

template <typename T>
inline void Read(T &var, const u32 addr) {
    switch (addr) {

    // Bit 0 set for Retail
    case UNITINFO:
        var = 0x00000001;
        break;

    // Set app memory size to 64MB?
    case APPMEMALLOC:
        var = 0x04000000;
        break;

    // Unknown - normally set to: 0x08000000 - (APPMEMALLOC + *0x1FF80048)
    // (Total FCRAM size - APPMEMALLOC - *0x1FF80048)
    case 0x1FF80044:
        var = 0x08000000 - (0x04000000 + 0x1400000);
        break;

    // Unknown - normally set to: 0x1400000 (20MB)
    case 0x1FF80048:
        var = 0x1400000;
        break;

    default:
        LOG_ERROR(Kernel, "unknown addr=0x%08X", addr);
    }
}

// Explicitly instantiate template functions because we aren't defining this in the header:

template void Read<u64>(u64 &var, const u32 addr);
template void Read<u32>(u32 &var, const u32 addr);
template void Read<u16>(u16 &var, const u32 addr);
template void Read<u8>(u8 &var, const u32 addr);


} // namespace
