// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>

#include "common/common_types.h"
#include "common/logging/log.h"

#include "core/hw/hw.h"
#include "core/hw/lcd.h"

#include "core/tracer/recorder.h"
#include "video_core/debug_utils/debug_utils.h"

namespace LCD {

Regs g_regs;

template <typename T>
inline void Read(T &var, const u32 raw_addr) {
    u32 addr = raw_addr - HW::VADDR_LCD;
    u32 index = addr / 4;

    // Reads other than u32 are untested, so I'd rather have them abort than silently fail
    if (index >= 0x400 || !std::is_same<T, u32>::value) {
        LOG_ERROR(HW_LCD, "unknown Read%lu @ 0x%08X", sizeof(var) * 8, addr);
        return;
    }

    var = g_regs[index];
}

template <typename T>
inline void Write(u32 addr, const T data) {
    addr -= HW::VADDR_LCD;
    u32 index = addr / 4;

    // Writes other than u32 are untested, so I'd rather have them abort than silently fail
    if (index >= 0x400 || !std::is_same<T, u32>::value) {
        LOG_ERROR(HW_LCD, "unknown Write%lu 0x%08X @ 0x%08X", sizeof(data) * 8, (u32)data, addr);
        return;
    }

    g_regs[index] = static_cast<u32>(data);

    // Notify tracer about the register write
    // This is happening *after* handling the write to make sure we properly catch all memory reads.
    if (Pica::g_debug_context && Pica::g_debug_context->recorder) {
        // addr + GPU VBase - IO VBase + IO PBase
        Pica::g_debug_context->recorder->RegisterWritten<T>(addr + HW::VADDR_LCD - 0x1EC00000 + 0x10100000, data);
    }
}

// Explicitly instantiate template functions because we aren't defining this in the header:

template void Read<u64>(u64 &var, const u32 addr);
template void Read<u32>(u32 &var, const u32 addr);
template void Read<u16>(u16 &var, const u32 addr);
template void Read<u8>(u8 &var, const u32 addr);

template void Write<u64>(u32 addr, const u64 data);
template void Write<u32>(u32 addr, const u32 data);
template void Write<u16>(u32 addr, const u16 data);
template void Write<u8>(u32 addr, const u8 data);

/// Initialize hardware
void Init() {
    memset(&g_regs, 0, sizeof(g_regs));
    LOG_DEBUG(HW_LCD, "initialized OK");
}

/// Shutdown hardware
void Shutdown() {
    LOG_DEBUG(HW_LCD, "shutdown OK");
}

} // namespace
