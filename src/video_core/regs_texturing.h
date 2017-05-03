// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "common/assert.h"
#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"

namespace Pica {

struct TexturingRegs {
    struct TextureConfig {
        enum TextureType : u32 {
            Texture2D = 0,
            TextureCube = 1,
            Shadow2D = 2,
            Projection2D = 3,
            ShadowCube = 4,
            Disabled = 5,
        };

        enum WrapMode : u32 {
            ClampToEdge = 0,
            ClampToBorder = 1,
            Repeat = 2,
            MirroredRepeat = 3,
        };

        enum TextureFilter : u32 {
            Nearest = 0,
            Linear = 1,
        };

        union {
            u32 raw;
            BitField<0, 8, u32> r;
            BitField<8, 8, u32> g;
            BitField<16, 8, u32> b;
            BitField<24, 8, u32> a;
        } border_color;

        union {
            BitField<0, 16, u32> height;
            BitField<16, 16, u32> width;
        };

        union {
            BitField<1, 1, TextureFilter> mag_filter;
            BitField<2, 1, TextureFilter> min_filter;
            BitField<8, 2, WrapMode> wrap_t;
            BitField<12, 2, WrapMode> wrap_s;
            BitField<28, 2, TextureType>
                type; ///< @note Only valid for texture 0 according to 3DBrew.
        };

        INSERT_PADDING_WORDS(0x1);

        u32 address;

        PAddr GetPhysicalAddress() const {
            return address * 8;
        }

        // texture1 and texture2 store the texture format directly after the address
        // whereas texture0 inserts some additional flags inbetween.
        // Hence, we store the format separately so that all other parameters can be described
        // in a single structure.
    };

    enum class TextureFormat : u32 {
        RGBA8 = 0,
        RGB8 = 1,
        RGB5A1 = 2,
        RGB565 = 3,
        RGBA4 = 4,
        IA8 = 5,
        RG8 = 6, ///< @note Also called HILO8 in 3DBrew.
        I8 = 7,
        A8 = 8,
        IA4 = 9,
        I4 = 10,
        A4 = 11,
        ETC1 = 12,   // compressed
        ETC1A4 = 13, // compressed
    };

    static unsigned NibblesPerPixel(TextureFormat format) {
        switch (format) {
        case TextureFormat::RGBA8:
            return 8;

        case TextureFormat::RGB8:
            return 6;

        case TextureFormat::RGB5A1:
        case TextureFormat::RGB565:
        case TextureFormat::RGBA4:
        case TextureFormat::IA8:
        case TextureFormat::RG8:
            return 4;

        case TextureFormat::I4:
        case TextureFormat::A4:
            return 1;

        case TextureFormat::I8:
        case TextureFormat::A8:
        case TextureFormat::IA4:

        default: // placeholder for yet unknown formats
            UNIMPLEMENTED();
            return 0;
        }
    }

    union {
        BitField<0, 1, u32> texture0_enable;
        BitField<1, 1, u32> texture1_enable;
        BitField<2, 1, u32> texture2_enable;
        BitField<8, 2, u32> texture3_coordinates; // TODO: unimplemented
        BitField<10, 1, u32> texture3_enable;     // TODO: unimplemented
        BitField<13, 1, u32> texture2_use_coord1;
        BitField<16, 1, u32> clear_texture_cache; // TODO: unimplemented
    };
    TextureConfig texture0;
    INSERT_PADDING_WORDS(0x8);
    BitField<0, 4, TextureFormat> texture0_format;
    BitField<0, 1, u32> fragment_lighting_enable;
    INSERT_PADDING_WORDS(0x1);
    TextureConfig texture1;
    BitField<0, 4, TextureFormat> texture1_format;
    INSERT_PADDING_WORDS(0x2);
    TextureConfig texture2;
    BitField<0, 4, TextureFormat> texture2_format;
    INSERT_PADDING_WORDS(0x21);

    struct FullTextureConfig {
        const bool enabled;
        const TextureConfig config;
        const TextureFormat format;
    };
    const std::array<FullTextureConfig, 3> GetTextures() const {
        return {{
            {texture0_enable.ToBool(), texture0, texture0_format},
            {texture1_enable.ToBool(), texture1, texture1_format},
            {texture2_enable.ToBool(), texture2, texture2_format},
        }};
    }

    // 0xc0-0xff: Texture Combiner (akin to glTexEnv)
    struct TevStageConfig {
        enum class Source : u32 {
            PrimaryColor = 0x0,
            PrimaryFragmentColor = 0x1,
            SecondaryFragmentColor = 0x2,

            Texture0 = 0x3,
            Texture1 = 0x4,
            Texture2 = 0x5,
            Texture3 = 0x6,

            PreviousBuffer = 0xd,
            Constant = 0xe,
            Previous = 0xf,
        };

        enum class ColorModifier : u32 {
            SourceColor = 0x0,
            OneMinusSourceColor = 0x1,
            SourceAlpha = 0x2,
            OneMinusSourceAlpha = 0x3,
            SourceRed = 0x4,
            OneMinusSourceRed = 0x5,

            SourceGreen = 0x8,
            OneMinusSourceGreen = 0x9,

            SourceBlue = 0xc,
            OneMinusSourceBlue = 0xd,
        };

        enum class AlphaModifier : u32 {
            SourceAlpha = 0x0,
            OneMinusSourceAlpha = 0x1,
            SourceRed = 0x2,
            OneMinusSourceRed = 0x3,
            SourceGreen = 0x4,
            OneMinusSourceGreen = 0x5,
            SourceBlue = 0x6,
            OneMinusSourceBlue = 0x7,
        };

        enum class Operation : u32 {
            Replace = 0,
            Modulate = 1,
            Add = 2,
            AddSigned = 3,
            Lerp = 4,
            Subtract = 5,
            Dot3_RGB = 6,
            Dot3_RGBA = 7,
            MultiplyThenAdd = 8,
            AddThenMultiply = 9,
        };

        union {
            u32 sources_raw;
            BitField<0, 4, Source> color_source1;
            BitField<4, 4, Source> color_source2;
            BitField<8, 4, Source> color_source3;
            BitField<16, 4, Source> alpha_source1;
            BitField<20, 4, Source> alpha_source2;
            BitField<24, 4, Source> alpha_source3;
        };

        union {
            u32 modifiers_raw;
            BitField<0, 4, ColorModifier> color_modifier1;
            BitField<4, 4, ColorModifier> color_modifier2;
            BitField<8, 4, ColorModifier> color_modifier3;
            BitField<12, 3, AlphaModifier> alpha_modifier1;
            BitField<16, 3, AlphaModifier> alpha_modifier2;
            BitField<20, 3, AlphaModifier> alpha_modifier3;
        };

        union {
            u32 ops_raw;
            BitField<0, 4, Operation> color_op;
            BitField<16, 4, Operation> alpha_op;
        };

        union {
            u32 const_color;
            BitField<0, 8, u32> const_r;
            BitField<8, 8, u32> const_g;
            BitField<16, 8, u32> const_b;
            BitField<24, 8, u32> const_a;
        };

        union {
            u32 scales_raw;
            BitField<0, 2, u32> color_scale;
            BitField<16, 2, u32> alpha_scale;
        };

        inline unsigned GetColorMultiplier() const {
            return (color_scale < 3) ? (1 << color_scale) : 1;
        }

        inline unsigned GetAlphaMultiplier() const {
            return (alpha_scale < 3) ? (1 << alpha_scale) : 1;
        }
    };

    TevStageConfig tev_stage0;
    INSERT_PADDING_WORDS(0x3);
    TevStageConfig tev_stage1;
    INSERT_PADDING_WORDS(0x3);
    TevStageConfig tev_stage2;
    INSERT_PADDING_WORDS(0x3);
    TevStageConfig tev_stage3;
    INSERT_PADDING_WORDS(0x3);

    enum class FogMode : u32 {
        None = 0,
        Fog = 5,
        Gas = 7,
    };

    union {
        BitField<0, 3, FogMode> fog_mode;
        BitField<16, 1, u32> fog_flip;

        union {
            // Tev stages 0-3 write their output to the combiner buffer if the corresponding bit in
            // these masks are set
            BitField<8, 4, u32> update_mask_rgb;
            BitField<12, 4, u32> update_mask_a;

            bool TevStageUpdatesCombinerBufferColor(unsigned stage_index) const {
                return (stage_index < 4) && (update_mask_rgb & (1 << stage_index));
            }

            bool TevStageUpdatesCombinerBufferAlpha(unsigned stage_index) const {
                return (stage_index < 4) && (update_mask_a & (1 << stage_index));
            }
        } tev_combiner_buffer_input;
    };

    union {
        u32 raw;
        BitField<0, 8, u32> r;
        BitField<8, 8, u32> g;
        BitField<16, 8, u32> b;
    } fog_color;

    INSERT_PADDING_WORDS(0x4);

    BitField<0, 16, u32> fog_lut_offset;

    INSERT_PADDING_WORDS(0x1);

    u32 fog_lut_data[8];

    TevStageConfig tev_stage4;
    INSERT_PADDING_WORDS(0x3);
    TevStageConfig tev_stage5;

    union {
        u32 raw;
        BitField<0, 8, u32> r;
        BitField<8, 8, u32> g;
        BitField<16, 8, u32> b;
        BitField<24, 8, u32> a;
    } tev_combiner_buffer_color;

    INSERT_PADDING_WORDS(0x2);

    const std::array<TevStageConfig, 6> GetTevStages() const {
        return {{tev_stage0, tev_stage1, tev_stage2, tev_stage3, tev_stage4, tev_stage5}};
    };
};

static_assert(sizeof(TexturingRegs) == 0x80 * sizeof(u32),
              "TexturingRegs struct has incorrect size");

} // namespace Pica
