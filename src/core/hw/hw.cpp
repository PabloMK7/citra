// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/log.h"

#include "core/hw/hw.h"
#include "core/hw/hw_lcd.h"

namespace HW {

template <typename T>
inline void Read(T &var, const u32 addr) {
    NOTICE_LOG(HW, "read from address %08X", addr);
}

template <typename T>
inline void Write(u32 addr, const T data) {
    NOTICE_LOG(HW, "write to address %08X", addr);
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
    LCD::Update();
}

/// Initialize hardware
void Init() {
    LCD::Init();
    NOTICE_LOG(HW, "initialized OK");
}

/// Shutdown hardware
void Shutdown() {
    NOTICE_LOG(HW, "shutdown OK");
}

}