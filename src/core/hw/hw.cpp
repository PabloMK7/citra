// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common_types.h"

#include "core/hw/hw.h"
#include "core/hw/gpu.h"
#include "core/hw/ndma.h"

namespace HW {

enum {
    VADDR_HASH      = 0x1EC01000,
    VADDR_CSND      = 0x1EC03000,
    VADDR_DSP       = 0x1EC40000,
    VADDR_PDN       = 0x1EC41000,
    VADDR_CODEC     = 0x1EC41000,
    VADDR_SPI       = 0x1EC42000,
    VADDR_SPI_2     = 0x1EC43000,   // Only used under TWL_FIRM?
    VADDR_I2C       = 0x1EC44000,
    VADDR_CODEC_2   = 0x1EC45000,
    VADDR_HID       = 0x1EC46000,
    VADDR_PAD       = 0x1EC46000,
    VADDR_PTM       = 0x1EC46000,
    VADDR_GPIO      = 0x1EC47000,
    VADDR_I2C_2     = 0x1EC48000,
    VADDR_SPI_3     = 0x1EC60000,
    VADDR_I2C_3     = 0x1EC61000,
    VADDR_MIC       = 0x1EC62000,
    VADDR_PXI       = 0x1EC63000,   // 0xFFFD2000
    //VADDR_NTRCARD
    VADDR_CDMA      = 0xFFFDA000,   // CoreLink DMA-330? Info
    VADDR_DSP_2     = 0x1ED03000,
    VADDR_HASH_2    = 0x1EE01000,
    VADDR_GPU       = 0x1EF00000,
};

template <typename T>
inline void Read(T &var, const u32 addr) {
    switch (addr & 0xFFFFF000) {
    
    // TODO(bunnei): What is the virtual address of NDMA?
    // case VADDR_NDMA:
    //     NDMA::Read(var, addr);
    //     break;

    case VADDR_GPU:
        GPU::Read(var, addr);
        break;

    default:
        ERROR_LOG(HW, "unknown Read%lu @ 0x%08X", sizeof(var) * 8, addr);
    }
}

template <typename T>
inline void Write(u32 addr, const T data) {
    switch (addr & 0xFFFFF000) {
    
    // TODO(bunnei): What is the virtual address of NDMA?
    // case VADDR_NDMA 
    //     NDMA::Write(addr, data);
    //     break;

    case VADDR_GPU:
        GPU::Write(addr, data);
        break;

    default:
        ERROR_LOG(HW, "unknown Write%lu 0x%08X @ 0x%08X", sizeof(data) * 8, data, addr);
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
    GPU::Update();
    NDMA::Update();
}

/// Initialize hardware
void Init() {
    GPU::Init();
    NDMA::Init();
    NOTICE_LOG(HW, "initialized OK");
}

/// Shutdown hardware
void Shutdown() {
    NOTICE_LOG(HW, "shutdown OK");
}

}