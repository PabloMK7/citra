// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/logging/log.h"

#include "core/hw/hw.h"
#include "core/hw/gpu.h"
#include "core/hw/lcd.h"

namespace HW {

template <typename T>
inline void Read(T &var, const u32 addr) {
    switch (addr & 0xFFFFF000) {
    case VADDR_GPU:
    case VADDR_GPU + 0x1000:
    case VADDR_GPU + 0x2000:
    case VADDR_GPU + 0x3000:
    case VADDR_GPU + 0x4000:
    case VADDR_GPU + 0x5000:
    case VADDR_GPU + 0x6000:
    case VADDR_GPU + 0x7000:
    case VADDR_GPU + 0x8000:
    case VADDR_GPU + 0x9000:
    case VADDR_GPU + 0xA000:
    case VADDR_GPU + 0xB000:
    case VADDR_GPU + 0xC000:
    case VADDR_GPU + 0xD000:
    case VADDR_GPU + 0xE000:
    case VADDR_GPU + 0xF000:
        GPU::Read(var, addr);
        break;
    case VADDR_LCD:
        LCD::Read(var, addr);
        break;
    default:
        LOG_ERROR(HW_Memory, "unknown Read%lu @ 0x%08X", sizeof(var) * 8, addr);
    }
}

template <typename T>
inline void Write(u32 addr, const T data) {
    switch (addr & 0xFFFFF000) {
    case VADDR_GPU:
    case VADDR_GPU + 0x1000:
    case VADDR_GPU + 0x2000:
    case VADDR_GPU + 0x3000:
    case VADDR_GPU + 0x4000:
    case VADDR_GPU + 0x5000:
    case VADDR_GPU + 0x6000:
    case VADDR_GPU + 0x7000:
    case VADDR_GPU + 0x8000:
    case VADDR_GPU + 0x9000:
    case VADDR_GPU + 0xA000:
    case VADDR_GPU + 0xB000:
    case VADDR_GPU + 0xC000:
    case VADDR_GPU + 0xD000:
    case VADDR_GPU + 0xE000:
    case VADDR_GPU + 0xF000:
        GPU::Write(addr, data);
        break;
    case VADDR_LCD:
        LCD::Write(addr, data);
        break;
    default:
        LOG_ERROR(HW_Memory, "unknown Write%lu 0x%08X @ 0x%08X", sizeof(data) * 8, (u32)data, addr);
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

/// Update hardware
void Update() {
}

/// Initialize hardware
void Init() {
    GPU::Init();
    LCD::Init();
    LOG_DEBUG(HW, "initialized OK");
}

/// Shutdown hardware
void Shutdown() {
    GPU::Shutdown();
    LCD::Shutdown();
    LOG_DEBUG(HW, "shutdown OK");
}

}
