// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include "common/common_types.h"

namespace HW {
namespace AES {

enum KeySlotID : size_t {
    APTWrap = 0x31,

    MaxKeySlotID = 0x40,
};

constexpr size_t AES_BLOCK_SIZE = 16;

using AESKey = std::array<u8, AES_BLOCK_SIZE>;

void InitKeys();

void SetGeneratorConstant(const AESKey& key);
void SetKeyX(size_t slot_id, const AESKey& key);
void SetKeyY(size_t slot_id, const AESKey& key);
void SetNormalKey(size_t slot_id, const AESKey& key);

bool IsNormalKeyAvailable(size_t slot_id);
AESKey GetNormalKey(size_t slot_id);

} // namspace AES
} // namespace HW
