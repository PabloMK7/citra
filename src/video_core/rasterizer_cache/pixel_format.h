// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once
#include <string_view>
#include "core/hw/gpu.h"
#include "video_core/regs_texturing.h"
#include "video_core/regs_framebuffer.h"

namespace OpenGL {

constexpr u32 PIXEL_FORMAT_COUNT = 18;

enum class PixelFormat : u8 {
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

inline constexpr std::string_view PixelFormatAsString(PixelFormat format) {
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
        return "NotReal";
    }
}

inline constexpr PixelFormat PixelFormatFromTextureFormat(Pica::TexturingRegs::TextureFormat format) {
    const u32 format_index = static_cast<u32>(format);
    return (format_index < 14) ? static_cast<PixelFormat>(format) : PixelFormat::Invalid;
}

inline constexpr PixelFormat PixelFormatFromColorFormat(Pica::FramebufferRegs::ColorFormat format) {
    const u32 format_index = static_cast<u32>(format);
    return (format_index < 5) ? static_cast<PixelFormat>(format) : PixelFormat::Invalid;
}

inline PixelFormat PixelFormatFromDepthFormat(Pica::FramebufferRegs::DepthFormat format) {
    const u32 format_index = static_cast<u32>(format);
    return (format_index < 4) ? static_cast<PixelFormat>(format_index + 14)
                              : PixelFormat::Invalid;
}

inline constexpr PixelFormat PixelFormatFromGPUPixelFormat(GPU::Regs::PixelFormat format) {
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

static constexpr SurfaceType GetFormatType(PixelFormat pixel_format) {
    const u32 format_index = static_cast<u32>(pixel_format);
    if (format_index < 5) {
        return SurfaceType::Color;
    }

    if (format_index < 14) {
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

inline constexpr bool CheckFormatsBlittable(PixelFormat source_format, PixelFormat dest_format) {
    SurfaceType source_type = GetFormatType(source_format);
    SurfaceType dest_type = GetFormatType(dest_format);

    if ((source_type == SurfaceType::Color || source_type == SurfaceType::Texture) &&
        (dest_type == SurfaceType::Color || dest_type == SurfaceType::Texture)) {
        return true;
    }

    if (source_type == SurfaceType::Depth && dest_type == SurfaceType::Depth) {
        return true;
    }

    if (source_type == SurfaceType::DepthStencil && dest_type == SurfaceType::DepthStencil) {
        return true;
    }

    return false;
}

static constexpr u32 GetFormatBpp(PixelFormat format) {
    switch (format) {
    case PixelFormat::RGBA8:
    case PixelFormat::D24S8:
        return 32;
    case PixelFormat::RGB8:
    case PixelFormat::D24:
        return 24;
    case PixelFormat::RGB5A1:
    case PixelFormat::RGB565:
    case PixelFormat::RGBA4:
    case PixelFormat::IA8:
    case PixelFormat::RG8:
    case PixelFormat::D16:
        return 16;
    case PixelFormat::I8:
    case PixelFormat::A8:
    case PixelFormat::IA4:
    case PixelFormat::ETC1A4:
        return 8;
    case PixelFormat::I4:
    case PixelFormat::A4:
    case PixelFormat::ETC1:
        return 4;
    case PixelFormat::Invalid:
        return 0;
    }
}

inline constexpr u32 GetBytesPerPixel(PixelFormat format) {
    // OpenGL needs 4 bpp alignment for D24 since using GL_UNSIGNED_INT as type
    if (format == PixelFormat::D24 || GetFormatType(format) == SurfaceType::Texture) {
        return 4;
    }

    return GetFormatBpp(format) / 8;
}

} // namespace OpenGL
