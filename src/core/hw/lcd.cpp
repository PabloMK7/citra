// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/log.h"

#include "core/core.h"
#include "core/hw/lcd.h"

#include "video_core/video_core.h"

namespace LCD {

static const u32 kFrameTicks = 268123480 / 60;  ///< 268MHz / 60 frames per second

u64 g_last_ticks = 0; ///< Last CPU ticks

template <typename T>
inline void Read(T &var, const u32 addr) {
    ERROR_LOG(LCD, "unknown Read%d @ 0x%08X", sizeof(var) * 8, addr);
}

template <typename T>
inline void Write(u32 addr, const T data) {
    ERROR_LOG(LCD, "unknown Write%d 0x%08X @ 0x%08X", sizeof(data) * 8, data, addr);
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

/// Update hardware
void Update() {
    u64 current_ticks = Core::g_app_core->GetTicks();

    if ((current_ticks - g_last_ticks) >= kFrameTicks) {
        g_last_ticks = current_ticks;
        VideoCore::g_renderer->SwapBuffers();
    }
}

/// Initialize hardware
void Init() {
    g_last_ticks = Core::g_app_core->GetTicks();
    NOTICE_LOG(LCD, "initialized OK");
}

/// Shutdown hardware
void Shutdown() {
    NOTICE_LOG(LCD, "shutdown OK");
}

} // namespace
