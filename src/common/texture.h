// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "common/common_types.h"

namespace Common {
void FlipRGBA8Texture(std::vector<u8>& tex, u32 width, u32 height);
}
