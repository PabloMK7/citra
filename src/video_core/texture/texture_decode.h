// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "common/vector_math.h"
#include "video_core/pica.h"

namespace Pica {
namespace Texture {

/// Returns the byte size of a 8*8 tile of the specified texture format.
size_t CalculateTileSize(Pica::Regs::TextureFormat format);

struct TextureInfo {
    PAddr physical_address;
    unsigned int width;
    unsigned int height;
    ptrdiff_t stride;
    Pica::Regs::TextureFormat format;

    static TextureInfo FromPicaRegister(const Pica::Regs::TextureConfig& config,
                                        const Pica::Regs::TextureFormat& format);

    /// Calculates stride from format and width, assuming that the entire texture is contiguous.
    void SetDefaultStride() {
        stride = Pica::Texture::CalculateTileSize(format) * (width / 8);
    }
};

/**
 * Lookup texel located at the given coordinates and return an RGBA vector of its color.
 * @param source Source pointer to read data from
 * @param x,y Texture coordinates to read from
 * @param info TextureInfo object describing the texture setup
 * @param disable_alpha This is used for debug widgets which use this method to display textures
 * without providing a good way to visualize alpha by themselves. If true, this will return 255 for
 * the alpha component, and either drop the information entirely or store it in an "unused" color
 * channel.
 * @todo Eventually we should get rid of the disable_alpha parameter.
 */
Math::Vec4<u8> LookupTexture(const u8* source, unsigned int x, unsigned int y,
                             const TextureInfo& info, bool disable_alpha = false);

/**
 * Looks up a texel from a single 8x8 texture tile.
 *
 * @param source Pointer to the beginning of the tile.
 * @param x, y In-tile coordinates to read from. Must be < 8.
 * @param info TextureInfo describing the texture format.
 * @param disable_alpha Used for debugging. Sets the result alpha to 255 and either discards the
 *                      real alpha or inserts it in an otherwise unused channel.
 */
Math::Vec4<u8> LookupTexelInTile(const u8* source, unsigned int x, unsigned int y,
                                 const TextureInfo& info, bool disable_alpha);

} // namespace Texture
} // namespace Pica
