// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "common/vector_math.h"
#include "video_core/pica.h"

namespace Pica {
namespace Texture {

struct TextureInfo {
    PAddr physical_address;
    int width;
    int height;
    int stride;
    Pica::Regs::TextureFormat format;

    static TextureInfo FromPicaRegister(const Pica::Regs::TextureConfig& config,
                                        const Pica::Regs::TextureFormat& format);
};

/**
 * Lookup texel located at the given coordinates and return an RGBA vector of its color.
 * @param source Source pointer to read data from
 * @param s,t Texture coordinates to read from
 * @param info TextureInfo object describing the texture setup
 * @param disable_alpha This is used for debug widgets which use this method to display textures
 * without providing a good way to visualize alpha by themselves. If true, this will return 255 for
 * the alpha component, and either drop the information entirely or store it in an "unused" color
 * channel.
 * @todo Eventually we should get rid of the disable_alpha parameter.
 */
Math::Vec4<u8> LookupTexture(const u8* source, int s, int t, const TextureInfo& info,
                             bool disable_alpha = false);

} // namespace Texture
} // namespace Pica
