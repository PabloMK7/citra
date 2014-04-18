// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/log.h"

#include "core/hw/hw.h"
#include "core/hw/lcd.h"
#include "core/hw/ndma.h"

namespace HW {

enum {
    ADDRESS_CONFIG      = 0x10000000,
    ADDRESS_IRQ         = 0x10001000,
    ADDRESS_NDMA        = 0x10002000,
    ADDRESS_TIMER       = 0x10003000,
    ADDRESS_CTRCARD     = 0x10004000,
    ADDRESS_CTRCARD_2   = 0x10005000,
    ADDRESS_SDMC_NAND   = 0x10006000,
    ADDRESS_SDMC_NAND_2 = 0x10007000,   // Apparently not used on retail
    ADDRESS_PXI         = 0x10008000,
    ADDRESS_AES         = 0x10009000,
    ADDRESS_SHA         = 0x1000A000,
    ADDRESS_RSA         = 0x1000B000,
    ADDRESS_XDMA        = 0x1000C000,
    ADDRESS_SPICARD     = 0x1000D800,
    ADDRESS_CONFIG_2    = 0x10010000,
    ADDRESS_HASH        = 0x10101000,
    ADDRESS_CSND        = 0x10103000,
    ADDRESS_DSP         = 0x10140000,
    ADDRESS_PDN         = 0x10141000,
    ADDRESS_CODEC       = 0x10141000,
    ADDRESS_SPI         = 0x10142000,
    ADDRESS_SPI_2       = 0x10143000,
    ADDRESS_I2C         = 0x10144000,
    ADDRESS_CODEC_2     = 0x10145000,
    ADDRESS_HID         = 0x10146000,
    ADDRESS_PAD         = 0x10146000,
    ADDRESS_PTM         = 0x10146000,
    ADDRESS_I2C_2       = 0x10148000,
    ADDRESS_SPI_3       = 0x10160000,
    ADDRESS_I2C_3       = 0x10161000,
    ADDRESS_MIC         = 0x10162000,
    ADDRESS_PXI_2       = 0x10163000,
    ADDRESS_NTRCARD     = 0x10164000,
    ADDRESS_DSP_2       = 0x10203000,
    ADDRESS_HASH_2      = 0x10301000,
};

template <typename T>
inline void Read(T &var, const u32 addr) {
    switch (addr & 0xFFFFF000) {
    
    case ADDRESS_NDMA:
        NDMA::Read(var, addr);
        break;

    default:
        ERROR_LOG(HW, "unknown Read%d @ 0x%08X", sizeof(var) * 8, addr);
    }
}

template <typename T>
inline void Write(u32 addr, const T data) {
    switch (addr & 0xFFFFF000) {
    
    case ADDRESS_NDMA:
        NDMA::Write(addr, data);
        break;

    default:
        ERROR_LOG(HW, "unknown Write%d 0x%08X @ 0x%08X", sizeof(data) * 8, data, addr);
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
    LCD::Update();
    NDMA::Update();
}

/// Initialize hardware
void Init() {
    LCD::Init();
    NDMA::Init();
    NOTICE_LOG(HW, "initialized OK");
}

/// Shutdown hardware
void Shutdown() {
    NOTICE_LOG(HW, "shutdown OK");
}

}