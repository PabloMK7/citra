// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/log.h"

#include "core/core.h"
#include "core/hw/hw_lcd.h"

#include "video_core/video_core.h"

namespace LCD {

static const u32 kFrameTicks = 268123480 / 60;  ///< 268MHz / 60 frames per second

u64 g_last_ticks = 0; ///< Last CPU ticks

template <typename T>
inline void Read(T &var, const u32 addr) {
}

template <typename T>
inline void Write(u32 addr, const T data) {
}

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
    NOTICE_LOG(LCD, "LCD initialized OK");
}

/// Shutdown hardware
void Shutdown() {
    NOTICE_LOG(LCD, "LCD shutdown OK");
}

} // namespace
