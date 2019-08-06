#include <vector>
#include "common/assert.h"
#include "common/common_types.h"

namespace Common {
void FlipRGBA8Texture(std::vector<u8>& tex, u64 width, u64 height) {
    ASSERT(tex.size() == width * height * 4);
    const u64 line_size = width * 4;
    u8* temp_row = new u8[line_size];
    u32 offset_1;
    u32 offset_2;
    for (u64 line = 0; line < height / 2; line++) {
        offset_1 = line * line_size;
        offset_2 = (height - line - 1) * line_size;
        // Swap lines
        std::memcpy(temp_row, &tex[offset_1], line_size);
        std::memcpy(&tex[offset_1], &tex[offset_2], line_size);
        std::memcpy(&tex[offset_2], temp_row, line_size);
    }
    delete[] temp_row;
}
} // namespace Common