// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/assert.h"
#include "common/color.h"
#include "common/logging/log.h"
#include "common/math_util.h"
#include "common/vector_math.h"
#include "video_core/texture/texture_decode.h"
#include "video_core/utils.h"

namespace Pica {
namespace Texture {

Math::Vec4<u8> LookupTexture(const u8* source, int x, int y, const TextureInfo& info,
                             bool disable_alpha) {
    const unsigned int coarse_x = x & ~7;
    const unsigned int coarse_y = y & ~7;

    if (info.format != Regs::TextureFormat::ETC1 && info.format != Regs::TextureFormat::ETC1A4) {
        // TODO(neobrain): Fix code design to unify vertical block offsets!
        source += coarse_y * info.stride;
    }

    // TODO: Assert that width/height are multiples of block dimensions

    switch (info.format) {
    case Regs::TextureFormat::RGBA8: {
        auto res = Color::DecodeRGBA8(source + VideoCore::GetMortonOffset(x, y, 4));
        return {res.r(), res.g(), res.b(), static_cast<u8>(disable_alpha ? 255 : res.a())};
    }

    case Regs::TextureFormat::RGB8: {
        auto res = Color::DecodeRGB8(source + VideoCore::GetMortonOffset(x, y, 3));
        return {res.r(), res.g(), res.b(), 255};
    }

    case Regs::TextureFormat::RGB5A1: {
        auto res = Color::DecodeRGB5A1(source + VideoCore::GetMortonOffset(x, y, 2));
        return {res.r(), res.g(), res.b(), static_cast<u8>(disable_alpha ? 255 : res.a())};
    }

    case Regs::TextureFormat::RGB565: {
        auto res = Color::DecodeRGB565(source + VideoCore::GetMortonOffset(x, y, 2));
        return {res.r(), res.g(), res.b(), 255};
    }

    case Regs::TextureFormat::RGBA4: {
        auto res = Color::DecodeRGBA4(source + VideoCore::GetMortonOffset(x, y, 2));
        return {res.r(), res.g(), res.b(), static_cast<u8>(disable_alpha ? 255 : res.a())};
    }

    case Regs::TextureFormat::IA8: {
        const u8* source_ptr = source + VideoCore::GetMortonOffset(x, y, 2);

        if (disable_alpha) {
            // Show intensity as red, alpha as green
            return {source_ptr[1], source_ptr[0], 0, 255};
        } else {
            return {source_ptr[1], source_ptr[1], source_ptr[1], source_ptr[0]};
        }
    }

    case Regs::TextureFormat::RG8: {
        auto res = Color::DecodeRG8(source + VideoCore::GetMortonOffset(x, y, 2));
        return {res.r(), res.g(), 0, 255};
    }

    case Regs::TextureFormat::I8: {
        const u8* source_ptr = source + VideoCore::GetMortonOffset(x, y, 1);
        return {*source_ptr, *source_ptr, *source_ptr, 255};
    }

    case Regs::TextureFormat::A8: {
        const u8* source_ptr = source + VideoCore::GetMortonOffset(x, y, 1);

        if (disable_alpha) {
            return {*source_ptr, *source_ptr, *source_ptr, 255};
        } else {
            return {0, 0, 0, *source_ptr};
        }
    }

    case Regs::TextureFormat::IA4: {
        const u8* source_ptr = source + VideoCore::GetMortonOffset(x, y, 1);

        u8 i = Color::Convert4To8(((*source_ptr) & 0xF0) >> 4);
        u8 a = Color::Convert4To8((*source_ptr) & 0xF);

        if (disable_alpha) {
            // Show intensity as red, alpha as green
            return {i, a, 0, 255};
        } else {
            return {i, i, i, a};
        }
    }

    case Regs::TextureFormat::I4: {
        u32 morton_offset = VideoCore::GetMortonOffset(x, y, 1);
        const u8* source_ptr = source + morton_offset / 2;

        u8 i = (morton_offset % 2) ? ((*source_ptr & 0xF0) >> 4) : (*source_ptr & 0xF);
        i = Color::Convert4To8(i);

        return {i, i, i, 255};
    }

    case Regs::TextureFormat::A4: {
        u32 morton_offset = VideoCore::GetMortonOffset(x, y, 1);
        const u8* source_ptr = source + morton_offset / 2;

        u8 a = (morton_offset % 2) ? ((*source_ptr & 0xF0) >> 4) : (*source_ptr & 0xF);
        a = Color::Convert4To8(a);

        if (disable_alpha) {
            return {a, a, a, 255};
        } else {
            return {0, 0, 0, a};
        }
    }

    case Regs::TextureFormat::ETC1:
    case Regs::TextureFormat::ETC1A4: {
        bool has_alpha = (info.format == Regs::TextureFormat::ETC1A4);

        // ETC1 further subdivides each 8x8 tile into four 4x4 subtiles
        const int subtile_width = 4;
        const int subtile_height = 4;

        int subtile_index = ((x / subtile_width) & 1) + 2 * ((y / subtile_height) & 1);
        unsigned subtile_bytes = has_alpha ? 2 : 1; // TODO: Name...

        const u64* source_ptr = (const u64*)(source + coarse_x * subtile_bytes * 4 +
                                             coarse_y * subtile_bytes * 4 * (info.width / 8) +
                                             subtile_index * subtile_bytes * 8);
        u64 alpha = 0xFFFFFFFFFFFFFFFF;
        if (has_alpha) {
            alpha = *source_ptr;
            source_ptr++;
        }

        union ETC1Tile {
            // Each of these two is a collection of 16 bits (one per lookup value)
            BitField<0, 16, u64> table_subindexes;
            BitField<16, 16, u64> negation_flags;

            unsigned GetTableSubIndex(unsigned index) const {
                return (table_subindexes >> index) & 1;
            }

            bool GetNegationFlag(unsigned index) const {
                return ((negation_flags >> index) & 1) == 1;
            }

            BitField<32, 1, u64> flip;
            BitField<33, 1, u64> differential_mode;

            BitField<34, 3, u64> table_index_2;
            BitField<37, 3, u64> table_index_1;

            union {
                // delta value + base value
                BitField<40, 3, s64> db;
                BitField<43, 5, u64> b;

                BitField<48, 3, s64> dg;
                BitField<51, 5, u64> g;

                BitField<56, 3, s64> dr;
                BitField<59, 5, u64> r;
            } differential;

            union {
                BitField<40, 4, u64> b2;
                BitField<44, 4, u64> b1;

                BitField<48, 4, u64> g2;
                BitField<52, 4, u64> g1;

                BitField<56, 4, u64> r2;
                BitField<60, 4, u64> r1;
            } separate;

            const Math::Vec3<u8> GetRGB(int x, int y) const {
                int texel = 4 * x + y;

                if (flip)
                    std::swap(x, y);

                // Lookup base value
                Math::Vec3<int> ret;
                if (differential_mode) {
                    ret.r() = static_cast<int>(differential.r);
                    ret.g() = static_cast<int>(differential.g);
                    ret.b() = static_cast<int>(differential.b);
                    if (x >= 2) {
                        ret.r() += static_cast<int>(differential.dr);
                        ret.g() += static_cast<int>(differential.dg);
                        ret.b() += static_cast<int>(differential.db);
                    }
                    ret.r() = Color::Convert5To8(ret.r());
                    ret.g() = Color::Convert5To8(ret.g());
                    ret.b() = Color::Convert5To8(ret.b());
                } else {
                    if (x < 2) {
                        ret.r() = Color::Convert4To8(static_cast<u8>(separate.r1));
                        ret.g() = Color::Convert4To8(static_cast<u8>(separate.g1));
                        ret.b() = Color::Convert4To8(static_cast<u8>(separate.b1));
                    } else {
                        ret.r() = Color::Convert4To8(static_cast<u8>(separate.r2));
                        ret.g() = Color::Convert4To8(static_cast<u8>(separate.g2));
                        ret.b() = Color::Convert4To8(static_cast<u8>(separate.b2));
                    }
                }

                // Add modifier
                unsigned table_index =
                    static_cast<int>((x < 2) ? table_index_1.Value() : table_index_2.Value());

                static const std::array<std::array<u8, 2>, 8> etc1_modifier_table = {{
                    {{2, 8}},
                    {{5, 17}},
                    {{9, 29}},
                    {{13, 42}},
                    {{18, 60}},
                    {{24, 80}},
                    {{33, 106}},
                    {{47, 183}},
                }};

                int modifier = etc1_modifier_table.at(table_index).at(GetTableSubIndex(texel));
                if (GetNegationFlag(texel))
                    modifier *= -1;

                ret.r() = MathUtil::Clamp(ret.r() + modifier, 0, 255);
                ret.g() = MathUtil::Clamp(ret.g() + modifier, 0, 255);
                ret.b() = MathUtil::Clamp(ret.b() + modifier, 0, 255);

                return ret.Cast<u8>();
            }
        } const* etc1_tile = reinterpret_cast<const ETC1Tile*>(source_ptr);

        alpha >>= 4 * ((x & 3) * 4 + (y & 3));
        return Math::MakeVec(etc1_tile->GetRGB(x & 3, y & 3),
                             disable_alpha ? (u8)255 : Color::Convert4To8(alpha & 0xF));
    }

    default:
        LOG_ERROR(HW_GPU, "Unknown texture format: %x", (u32)info.format);
        DEBUG_ASSERT(false);
        return {};
    }
}

TextureInfo TextureInfo::FromPicaRegister(const Regs::TextureConfig& config,
                                          const Regs::TextureFormat& format) {
    TextureInfo info;
    info.physical_address = config.GetPhysicalAddress();
    info.width = config.width;
    info.height = config.height;
    info.format = format;
    info.stride = Pica::Regs::NibblesPerPixel(info.format) * info.width / 2;
    return info;
}

} // namespace Texture
} // namespace Pica
