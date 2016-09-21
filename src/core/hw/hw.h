// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace HW {

/// Beginnings of IO register regions, in the user VA space.
enum : u32 {
    VADDR_HASH = 0x1EC01000,
    VADDR_CSND = 0x1EC03000,
    VADDR_DSP = 0x1EC40000,
    VADDR_PDN = 0x1EC41000,
    VADDR_CODEC = 0x1EC41000,
    VADDR_SPI = 0x1EC42000,
    VADDR_SPI_2 = 0x1EC43000, // Only used under TWL_FIRM?
    VADDR_I2C = 0x1EC44000,
    VADDR_CODEC_2 = 0x1EC45000,
    VADDR_HID = 0x1EC46000,
    VADDR_GPIO = 0x1EC47000,
    VADDR_I2C_2 = 0x1EC48000,
    VADDR_SPI_3 = 0x1EC60000,
    VADDR_I2C_3 = 0x1EC61000,
    VADDR_MIC = 0x1EC62000,
    VADDR_PXI = 0x1EC63000,
    VADDR_LCD = 0x1ED02000,
    VADDR_DSP_2 = 0x1ED03000,
    VADDR_HASH_2 = 0x1EE01000,
    VADDR_GPU = 0x1EF00000,
};

template <typename T>
void Read(T& var, const u32 addr);

template <typename T>
void Write(u32 addr, const T data);

/// Update hardware
void Update();

/// Initialize hardware
void Init();

/// Shutdown hardware
void Shutdown();

} // namespace
