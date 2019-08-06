#pragma once

#include <vector>
#include "common/common_types.h"

namespace Common {
void FlipRGBA8Texture(std::vector<u8>& tex, u64 width, u64 height);
}