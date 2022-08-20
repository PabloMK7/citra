// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <type_traits>
#include <glad/glad.h>
#include "common/common_types.h"
#include "common/math_util.h"
#include "video_core/rasterizer_cache/pixel_format.h"

namespace OpenGL {

class RasterizerCacheOpenGL;

struct PixelFormatPair {
    const PixelFormat dst_format, src_format;

    struct less {
        using is_transparent = void;
        constexpr bool operator()(PixelFormatPair lhs, PixelFormatPair rhs) const {
            return std::tie(lhs.dst_format, lhs.src_format) <
                   std::tie(rhs.dst_format, rhs.src_format);
        }

        constexpr bool operator()(PixelFormat lhs, PixelFormatPair rhs) const {
            return lhs < rhs.dst_format;
        }

        constexpr bool operator()(PixelFormatPair lhs, PixelFormat rhs) const {
            return lhs.dst_format < rhs;
        }
    };
};

class FormatReinterpreterBase {
public:
    virtual ~FormatReinterpreterBase() = default;

    virtual void Reinterpret(GLuint src_tex, const Common::Rectangle<u32>& src_rect,
                             GLuint read_fb_handle, GLuint dst_tex,
                             const Common::Rectangle<u32>& dst_rect, GLuint draw_fb_handle) = 0;
};

class FormatReinterpreterOpenGL : NonCopyable {
    using ReinterpreterMap =
        std::map<PixelFormatPair, std::unique_ptr<FormatReinterpreterBase>, PixelFormatPair::less>;

public:
    explicit FormatReinterpreterOpenGL();
    ~FormatReinterpreterOpenGL();

    auto GetPossibleReinterpretations(PixelFormat dst_format) ->
    std::pair<ReinterpreterMap::iterator, ReinterpreterMap::iterator>;

private:
    ReinterpreterMap reinterpreters;
};

} // namespace OpenGL
