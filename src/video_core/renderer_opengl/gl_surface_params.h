// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <climits>
#include <boost/icl/interval.hpp>
#include "common/assert.h"
#include "common/math_util.h"
#include "core/hw/gpu.h"
#include "video_core/regs_framebuffer.h"
#include "video_core/regs_texturing.h"

namespace OpenGL {

struct CachedSurface;
using Surface = std::shared_ptr<CachedSurface>;

using SurfaceInterval = boost::icl::right_open_interval<PAddr>;

struct SurfaceParams {
private:
    static constexpr std::array<unsigned int, 18> BPP_TABLE = {
        32, // RGBA8
        24, // RGB8
        16, // RGB5A1
        16, // RGB565
        16, // RGBA4
        16, // IA8
        16, // RG8
        8,  // I8
        8,  // A8
        8,  // IA4
        4,  // I4
        4,  // A4
        4,  // ETC1
        8,  // ETC1A4
        16, // D16
        0,
        24, // D24
        32, // D24S8
    };

public:
    enum class PixelFormat {
        // First 5 formats are shared between textures and color buffers
        RGBA8 = 0,
        RGB8 = 1,
        RGB5A1 = 2,
        RGB565 = 3,
        RGBA4 = 4,

        // Texture-only formats
        IA8 = 5,
        RG8 = 6,
        I8 = 7,
        A8 = 8,
        IA4 = 9,
        I4 = 10,
        A4 = 11,
        ETC1 = 12,
        ETC1A4 = 13,

        // Depth buffer-only formats
        D16 = 14,
        // gap
        D24 = 16,
        D24S8 = 17,

        Invalid = 255,
    };

    enum class SurfaceType {
        Color = 0,
        Texture = 1,
        Depth = 2,
        DepthStencil = 3,
        Fill = 4,
        Invalid = 5
    };

    static constexpr unsigned int GetFormatBpp(PixelFormat format) {
        const auto format_idx = static_cast<std::size_t>(format);
        DEBUG_ASSERT_MSG(format_idx < BPP_TABLE.size(), "Invalid pixel format {}", format_idx);
        return BPP_TABLE[format_idx];
    }

    unsigned int GetFormatBpp() const {
        return GetFormatBpp(pixel_format);
    }

    static std::string_view PixelFormatAsString(PixelFormat format) {
        switch (format) {
        case PixelFormat::RGBA8:
            return "RGBA8";
        case PixelFormat::RGB8:
            return "RGB8";
        case PixelFormat::RGB5A1:
            return "RGB5A1";
        case PixelFormat::RGB565:
            return "RGB565";
        case PixelFormat::RGBA4:
            return "RGBA4";
        case PixelFormat::IA8:
            return "IA8";
        case PixelFormat::RG8:
            return "RG8";
        case PixelFormat::I8:
            return "I8";
        case PixelFormat::A8:
            return "A8";
        case PixelFormat::IA4:
            return "IA4";
        case PixelFormat::I4:
            return "I4";
        case PixelFormat::A4:
            return "A4";
        case PixelFormat::ETC1:
            return "ETC1";
        case PixelFormat::ETC1A4:
            return "ETC1A4";
        case PixelFormat::D16:
            return "D16";
        case PixelFormat::D24:
            return "D24";
        case PixelFormat::D24S8:
            return "D24S8";
        default:
            return "Not a real pixel format";
        }
    }

    static PixelFormat PixelFormatFromTextureFormat(Pica::TexturingRegs::TextureFormat format) {
        return ((unsigned int)format < 14) ? (PixelFormat)format : PixelFormat::Invalid;
    }

    static PixelFormat PixelFormatFromColorFormat(Pica::FramebufferRegs::ColorFormat format) {
        return ((unsigned int)format < 5) ? (PixelFormat)format : PixelFormat::Invalid;
    }

    static PixelFormat PixelFormatFromDepthFormat(Pica::FramebufferRegs::DepthFormat format) {
        return ((unsigned int)format < 4) ? (PixelFormat)((unsigned int)format + 14)
                                          : PixelFormat::Invalid;
    }

    static PixelFormat PixelFormatFromGPUPixelFormat(GPU::Regs::PixelFormat format) {
        switch (format) {
        // RGB565 and RGB5A1 are switched in PixelFormat compared to ColorFormat
        case GPU::Regs::PixelFormat::RGB565:
            return PixelFormat::RGB565;
        case GPU::Regs::PixelFormat::RGB5A1:
            return PixelFormat::RGB5A1;
        default:
            return ((unsigned int)format < 5) ? (PixelFormat)format : PixelFormat::Invalid;
        }
    }

    static bool CheckFormatsBlittable(PixelFormat pixel_format_a, PixelFormat pixel_format_b) {
        SurfaceType a_type = GetFormatType(pixel_format_a);
        SurfaceType b_type = GetFormatType(pixel_format_b);

        if ((a_type == SurfaceType::Color || a_type == SurfaceType::Texture) &&
            (b_type == SurfaceType::Color || b_type == SurfaceType::Texture)) {
            return true;
        }

        if (a_type == SurfaceType::Depth && b_type == SurfaceType::Depth) {
            return true;
        }

        if (a_type == SurfaceType::DepthStencil && b_type == SurfaceType::DepthStencil) {
            return true;
        }

        return false;
    }

    static constexpr SurfaceType GetFormatType(PixelFormat pixel_format) {
        if ((unsigned int)pixel_format < 5) {
            return SurfaceType::Color;
        }

        if ((unsigned int)pixel_format < 14) {
            return SurfaceType::Texture;
        }

        if (pixel_format == PixelFormat::D16 || pixel_format == PixelFormat::D24) {
            return SurfaceType::Depth;
        }

        if (pixel_format == PixelFormat::D24S8) {
            return SurfaceType::DepthStencil;
        }

        return SurfaceType::Invalid;
    }

    /// Update the params "size", "end" and "type" from the already set "addr", "width", "height"
    /// and "pixel_format"
    void UpdateParams() {
        if (stride == 0) {
            stride = width;
        }
        type = GetFormatType(pixel_format);
        size = !is_tiled ? BytesInPixels(stride * (height - 1) + width)
                         : BytesInPixels(stride * 8 * (height / 8 - 1) + width * 8);
        end = addr + size;
    }

    SurfaceInterval GetInterval() const {
        return SurfaceInterval(addr, end);
    }

    // Returns the outer rectangle containing "interval"
    SurfaceParams FromInterval(SurfaceInterval interval) const;

    SurfaceInterval GetSubRectInterval(Common::Rectangle<u32> unscaled_rect) const;

    // Returns the region of the biggest valid rectange within interval
    SurfaceInterval GetCopyableInterval(const Surface& src_surface) const;

    u32 GetScaledWidth() const {
        return width * res_scale;
    }

    u32 GetScaledHeight() const {
        return height * res_scale;
    }

    Common::Rectangle<u32> GetRect() const {
        return {0, height, width, 0};
    }

    Common::Rectangle<u32> GetScaledRect() const {
        return {0, GetScaledHeight(), GetScaledWidth(), 0};
    }

    u32 PixelsInBytes(u32 size) const {
        return size * CHAR_BIT / GetFormatBpp(pixel_format);
    }

    u32 BytesInPixels(u32 pixels) const {
        return pixels * GetFormatBpp(pixel_format) / CHAR_BIT;
    }

    bool ExactMatch(const SurfaceParams& other_surface) const;
    bool CanSubRect(const SurfaceParams& sub_surface) const;
    bool CanExpand(const SurfaceParams& expanded_surface) const;
    bool CanTexCopy(const SurfaceParams& texcopy_params) const;

    Common::Rectangle<u32> GetSubRect(const SurfaceParams& sub_surface) const;
    Common::Rectangle<u32> GetScaledSubRect(const SurfaceParams& sub_surface) const;

    PAddr addr = 0;
    PAddr end = 0;
    u32 size = 0;

    u32 width = 0;
    u32 height = 0;
    u32 stride = 0;
    u16 res_scale = 1;

    bool is_tiled = false;
    PixelFormat pixel_format = PixelFormat::Invalid;
    SurfaceType type = SurfaceType::Invalid;
};

} // namespace OpenGL
