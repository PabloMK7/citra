// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace LCD {

enum {
    TOP_ASPECT_X        = 0x5,
    TOP_ASPECT_Y        = 0x3,
    
    TOP_HEIGHT          = 240,
    TOP_WIDTH           = 400,
    BOTTOM_WIDTH        = 320,

    // Physical addresses in FCRAM used by ARM9 applications - these are correct for real hardware 
    FRAMEBUFFER_SEL     = 0x20184E59,
    TOP_LEFT_FRAME1     = 0x20184E60,
    TOP_LEFT_FRAME2     = 0x201CB370,
    TOP_RIGHT_FRAME1    = 0x20282160,
    TOP_RIGHT_FRAME2    = 0x202C8670,
    SUB_FRAME1          = 0x202118E0,
    SUB_FRAME2          = 0x20249CF0,

    // Physical addresses in VRAM - I'm not sure how these are actually allocated (so not real)
    VRAM_FRAMEBUFFER_SEL    = 0x18184E59,
    VRAM_TOP_LEFT_FRAME1    = 0x18184E60,
    VRAM_TOP_LEFT_FRAME2    = 0x181CB370,
    VRAM_TOP_RIGHT_FRAME1   = 0x18282160,
    VRAM_TOP_RIGHT_FRAME2   = 0x182C8670,
    VRAM_SUB_FRAME1         = 0x182118E0,
    VRAM_SUB_FRAME2         = 0x18249CF0,
};

template <typename T>
inline void Read(T &var, const u32 addr);

template <typename T>
inline void Write(u32 addr, const T data);

/// Update hardware
void Update();

/// Initialize hardware
void Init();

/// Shutdown hardware
void Shutdown();


} // namespace
