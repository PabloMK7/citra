// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <string>

#ifndef _MSC_VER
#include <type_traits> // for std::enable_if
#endif

#include "common/assert.h"
#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/vector_math.h"

namespace Pica {

// Returns index corresponding to the Regs member labeled by field_name
// TODO: Due to Visual studio bug 209229, offsetof does not return constant expressions
//       when used with array elements (e.g. PICA_REG_INDEX(vs_uniform_setup.set_value[1])).
//       For details cf.
//       https://connect.microsoft.com/VisualStudio/feedback/details/209229/offsetof-does-not-produce-a-constant-expression-for-array-members
//       Hopefully, this will be fixed sometime in the future.
//       For lack of better alternatives, we currently hardcode the offsets when constant
//       expressions are needed via PICA_REG_INDEX_WORKAROUND (on sane compilers, static_asserts
//       will then make sure the offsets indeed match the automatically calculated ones).
#define PICA_REG_INDEX(field_name) (offsetof(Pica::Regs, field_name) / sizeof(u32))
#if defined(_MSC_VER)
#define PICA_REG_INDEX_WORKAROUND(field_name, backup_workaround_index) (backup_workaround_index)
#else
// NOTE: Yeah, hacking in a static_assert here just to workaround the lacking MSVC compiler
//       really is this annoying. This macro just forwards its first argument to PICA_REG_INDEX
//       and then performs a (no-op) cast to size_t iff the second argument matches the expected
//       field offset. Otherwise, the compiler will fail to compile this code.
#define PICA_REG_INDEX_WORKAROUND(field_name, backup_workaround_index)                             \
    ((typename std::enable_if<backup_workaround_index == PICA_REG_INDEX(field_name),               \
                              size_t>::type)PICA_REG_INDEX(field_name))
#endif // _MSC_VER

struct Regs {

    INSERT_PADDING_WORDS(0x10);

    u32 trigger_irq;

    INSERT_PADDING_WORDS(0x2f);

    enum class CullMode : u32 {
        // Select which polygons are considered to be "frontfacing".
        KeepAll = 0,
        KeepClockWise = 1,
        KeepCounterClockWise = 2,
        // TODO: What does the third value imply?
    };

    union {
        BitField<0, 2, CullMode> cull_mode;
    };

    BitField<0, 24, u32> viewport_size_x;

    INSERT_PADDING_WORDS(0x1);

    BitField<0, 24, u32> viewport_size_y;

    INSERT_PADDING_WORDS(0x9);

    BitField<0, 24, u32> viewport_depth_range;      // float24
    BitField<0, 24, u32> viewport_depth_near_plane; // float24

    BitField<0, 3, u32> vs_output_total;

    union VSOutputAttributes {
        // Maps components of output vertex attributes to semantics
        enum Semantic : u32 {
            POSITION_X = 0,
            POSITION_Y = 1,
            POSITION_Z = 2,
            POSITION_W = 3,

            QUATERNION_X = 4,
            QUATERNION_Y = 5,
            QUATERNION_Z = 6,
            QUATERNION_W = 7,

            COLOR_R = 8,
            COLOR_G = 9,
            COLOR_B = 10,
            COLOR_A = 11,

            TEXCOORD0_U = 12,
            TEXCOORD0_V = 13,
            TEXCOORD1_U = 14,
            TEXCOORD1_V = 15,

            // TODO: Not verified
            VIEW_X = 18,
            VIEW_Y = 19,
            VIEW_Z = 20,

            TEXCOORD2_U = 22,
            TEXCOORD2_V = 23,

            INVALID = 31,
        };

        BitField<0, 5, Semantic> map_x;
        BitField<8, 5, Semantic> map_y;
        BitField<16, 5, Semantic> map_z;
        BitField<24, 5, Semantic> map_w;
    } vs_output_attributes[7];

    INSERT_PADDING_WORDS(0xe);

    enum class ScissorMode : u32 {
        Disabled = 0,
        Exclude = 1, // Exclude pixels inside the scissor box

        Include = 3 // Exclude pixels outside the scissor box
    };

    struct {
        BitField<0, 2, ScissorMode> mode;

        union {
            BitField<0, 16, u32> x1;
            BitField<16, 16, u32> y1;
        };

        union {
            BitField<0, 16, u32> x2;
            BitField<16, 16, u32> y2;
        };
    } scissor_test;

    union {
        BitField<0, 10, s32> x;
        BitField<16, 10, s32> y;
    } viewport_corner;

    INSERT_PADDING_WORDS(0x1);

    // TODO: early depth
    INSERT_PADDING_WORDS(0x1);

    INSERT_PADDING_WORDS(0x2);

    enum DepthBuffering : u32 {
        WBuffering = 0,
        ZBuffering = 1,
    };
    BitField<0, 1, DepthBuffering> depthmap_enable;

    INSERT_PADDING_WORDS(0x12);

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

        u32 GetPhysicalAddress() const {
            return DecodeAddressRegister(address);
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

    enum class LogicOp : u32 {
        Clear = 0,
        And = 1,
        AndReverse = 2,
        Copy = 3,
        Set = 4,
        CopyInverted = 5,
        NoOp = 6,
        Invert = 7,
        Nand = 8,
        Or = 9,
        Nor = 10,
        Xor = 11,
        Equiv = 12,
        AndInverted = 13,
        OrReverse = 14,
        OrInverted = 15,
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
            return 2;
        }
    }

    union {
        BitField<0, 1, u32> texture0_enable;
        BitField<1, 1, u32> texture1_enable;
        BitField<2, 1, u32> texture2_enable;
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

    const std::array<Regs::TevStageConfig, 6> GetTevStages() const {
        return {{tev_stage0, tev_stage1, tev_stage2, tev_stage3, tev_stage4, tev_stage5}};
    };

    enum class BlendEquation : u32 {
        Add = 0,
        Subtract = 1,
        ReverseSubtract = 2,
        Min = 3,
        Max = 4,
    };

    enum class BlendFactor : u32 {
        Zero = 0,
        One = 1,
        SourceColor = 2,
        OneMinusSourceColor = 3,
        DestColor = 4,
        OneMinusDestColor = 5,
        SourceAlpha = 6,
        OneMinusSourceAlpha = 7,
        DestAlpha = 8,
        OneMinusDestAlpha = 9,
        ConstantColor = 10,
        OneMinusConstantColor = 11,
        ConstantAlpha = 12,
        OneMinusConstantAlpha = 13,
        SourceAlphaSaturate = 14,
    };

    enum class CompareFunc : u32 {
        Never = 0,
        Always = 1,
        Equal = 2,
        NotEqual = 3,
        LessThan = 4,
        LessThanOrEqual = 5,
        GreaterThan = 6,
        GreaterThanOrEqual = 7,
    };

    enum class StencilAction : u32 {
        Keep = 0,
        Zero = 1,
        Replace = 2,
        Increment = 3,
        Decrement = 4,
        Invert = 5,
        IncrementWrap = 6,
        DecrementWrap = 7,
    };

    struct {
        union {
            // If false, logic blending is used
            BitField<8, 1, u32> alphablend_enable;
        };

        union {
            BitField<0, 8, BlendEquation> blend_equation_rgb;
            BitField<8, 8, BlendEquation> blend_equation_a;

            BitField<16, 4, BlendFactor> factor_source_rgb;
            BitField<20, 4, BlendFactor> factor_dest_rgb;

            BitField<24, 4, BlendFactor> factor_source_a;
            BitField<28, 4, BlendFactor> factor_dest_a;
        } alpha_blending;

        union {
            BitField<0, 4, LogicOp> logic_op;
        };

        union {
            u32 raw;
            BitField<0, 8, u32> r;
            BitField<8, 8, u32> g;
            BitField<16, 8, u32> b;
            BitField<24, 8, u32> a;
        } blend_const;

        union {
            BitField<0, 1, u32> enable;
            BitField<4, 3, CompareFunc> func;
            BitField<8, 8, u32> ref;
        } alpha_test;

        struct {
            union {
                // Raw value of this register
                u32 raw_func;

                // If true, enable stencil testing
                BitField<0, 1, u32> enable;

                // Comparison operation for stencil testing
                BitField<4, 3, CompareFunc> func;

                // Mask used to control writing to the stencil buffer
                BitField<8, 8, u32> write_mask;

                // Value to compare against for stencil testing
                BitField<16, 8, u32> reference_value;

                // Mask to apply on stencil test inputs
                BitField<24, 8, u32> input_mask;
            };

            union {
                // Raw value of this register
                u32 raw_op;

                // Action to perform when the stencil test fails
                BitField<0, 3, StencilAction> action_stencil_fail;

                // Action to perform when stencil testing passed but depth testing fails
                BitField<4, 3, StencilAction> action_depth_fail;

                // Action to perform when both stencil and depth testing pass
                BitField<8, 3, StencilAction> action_depth_pass;
            };
        } stencil_test;

        union {
            BitField<0, 1, u32> depth_test_enable;
            BitField<4, 3, CompareFunc> depth_test_func;
            BitField<8, 1, u32> red_enable;
            BitField<9, 1, u32> green_enable;
            BitField<10, 1, u32> blue_enable;
            BitField<11, 1, u32> alpha_enable;
            BitField<12, 1, u32> depth_write_enable;
        };

        INSERT_PADDING_WORDS(0x8);
    } output_merger;

    // Components are laid out in reverse byte order, most significant bits first.
    enum class ColorFormat : u32 {
        RGBA8 = 0,
        RGB8 = 1,
        RGB5A1 = 2,
        RGB565 = 3,
        RGBA4 = 4,
    };

    enum class DepthFormat : u32 {
        D16 = 0,
        D24 = 2,
        D24S8 = 3,
    };

    // Returns the number of bytes in the specified color format
    static unsigned BytesPerColorPixel(ColorFormat format) {
        switch (format) {
        case ColorFormat::RGBA8:
            return 4;
        case ColorFormat::RGB8:
            return 3;
        case ColorFormat::RGB5A1:
        case ColorFormat::RGB565:
        case ColorFormat::RGBA4:
            return 2;
        default:
            LOG_CRITICAL(HW_GPU, "Unknown color format %u", format);
            UNIMPLEMENTED();
        }
    }

    struct FramebufferConfig {
        INSERT_PADDING_WORDS(0x3);

        union {
            BitField<0, 4, u32> allow_color_write; // 0 = disable, else enable
        };

        INSERT_PADDING_WORDS(0x1);

        union {
            BitField<0, 2, u32> allow_depth_stencil_write; // 0 = disable, else enable
        };

        DepthFormat depth_format; // TODO: Should be a BitField!
        BitField<16, 3, ColorFormat> color_format;

        INSERT_PADDING_WORDS(0x4);

        u32 depth_buffer_address;
        u32 color_buffer_address;

        union {
            // Apparently, the framebuffer width is stored as expected,
            // while the height is stored as the actual height minus one.
            // Hence, don't access these fields directly but use the accessors
            // GetWidth() and GetHeight() instead.
            BitField<0, 11, u32> width;
            BitField<12, 10, u32> height;
        };

        INSERT_PADDING_WORDS(0x1);

        inline u32 GetColorBufferPhysicalAddress() const {
            return DecodeAddressRegister(color_buffer_address);
        }
        inline u32 GetDepthBufferPhysicalAddress() const {
            return DecodeAddressRegister(depth_buffer_address);
        }

        inline u32 GetWidth() const {
            return width;
        }

        inline u32 GetHeight() const {
            return height + 1;
        }
    } framebuffer;

    // Returns the number of bytes in the specified depth format
    static u32 BytesPerDepthPixel(DepthFormat format) {
        switch (format) {
        case DepthFormat::D16:
            return 2;
        case DepthFormat::D24:
            return 3;
        case DepthFormat::D24S8:
            return 4;
        default:
            LOG_CRITICAL(HW_GPU, "Unknown depth format %u", format);
            UNIMPLEMENTED();
        }
    }

    // Returns the number of bits per depth component of the specified depth format
    static u32 DepthBitsPerPixel(DepthFormat format) {
        switch (format) {
        case DepthFormat::D16:
            return 16;
        case DepthFormat::D24:
        case DepthFormat::D24S8:
            return 24;
        default:
            LOG_CRITICAL(HW_GPU, "Unknown depth format %u", format);
            UNIMPLEMENTED();
        }
    }

    INSERT_PADDING_WORDS(0x20);

    enum class LightingSampler {
        Distribution0 = 0,
        Distribution1 = 1,
        Fresnel = 3,
        ReflectBlue = 4,
        ReflectGreen = 5,
        ReflectRed = 6,
        SpotlightAttenuation = 8,
        DistanceAttenuation = 16,
    };

    /**
     * Pica fragment lighting supports using different LUTs for each lighting component:
     * Reflectance R, G, and B channels, distribution function for specular components 0 and 1,
     * fresnel factor, and spotlight attenuation. Furthermore, which LUTs are used for each channel
     * (or whether a channel is enabled at all) is specified by various pre-defined lighting
     * configurations. With configurations that require more LUTs, more cycles are required on HW to
     * perform lighting computations.
     */
    enum class LightingConfig {
        Config0 = 0, ///< Reflect Red, Distribution 0, Spotlight
        Config1 = 1, ///< Reflect Red, Fresnel, Spotlight
        Config2 = 2, ///< Reflect Red, Distribution 0/1
        Config3 = 3, ///< Distribution 0/1, Fresnel
        Config4 = 4, ///< Reflect Red/Green/Blue, Distribution 0/1, Spotlight
        Config5 = 5, ///< Reflect Red/Green/Blue, Distribution 0, Fresnel, Spotlight
        Config6 = 6, ///< Reflect Red, Distribution 0/1, Fresnel, Spotlight
        Config7 = 8, ///< Reflect Red/Green/Blue, Distribution 0/1, Fresnel, Spotlight
                     ///< NOTE: '8' is intentional, '7' does not appear to be a valid configuration
    };

    /// Selects which lighting components are affected by fresnel
    enum class LightingFresnelSelector {
        None = 0,           ///< Fresnel is disabled
        PrimaryAlpha = 1,   ///< Primary (diffuse) lighting alpha is affected by fresnel
        SecondaryAlpha = 2, ///< Secondary (specular) lighting alpha is affected by fresnel
        Both =
            PrimaryAlpha |
            SecondaryAlpha, ///< Both primary and secondary lighting alphas are affected by fresnel
    };

    /// Factor used to scale the output of a lighting LUT
    enum class LightingScale {
        Scale1 = 0,   ///< Scale is 1x
        Scale2 = 1,   ///< Scale is 2x
        Scale4 = 2,   ///< Scale is 4x
        Scale8 = 3,   ///< Scale is 8x
        Scale1_4 = 6, ///< Scale is 0.25x
        Scale1_2 = 7, ///< Scale is 0.5x
    };

    enum class LightingLutInput {
        NH = 0, // Cosine of the angle between the normal and half-angle vectors
        VH = 1, // Cosine of the angle between the view and half-angle vectors
        NV = 2, // Cosine of the angle between the normal and the view vector
        LN = 3, // Cosine of the angle between the light and the normal vectors
    };

    enum class LightingBumpMode : u32 {
        None = 0,
        NormalMap = 1,
        TangentMap = 2,
    };

    union LightColor {
        BitField<0, 10, u32> b;
        BitField<10, 10, u32> g;
        BitField<20, 10, u32> r;

        Math::Vec3f ToVec3f() const {
            // These fields are 10 bits wide, however 255 corresponds to 1.0f for each color
            // component
            return Math::MakeVec((f32)r / 255.f, (f32)g / 255.f, (f32)b / 255.f);
        }
    };

    /// Returns true if the specified lighting sampler is supported by the current Pica lighting
    /// configuration
    static bool IsLightingSamplerSupported(LightingConfig config, LightingSampler sampler) {
        switch (sampler) {
        case LightingSampler::Distribution0:
            return (config != LightingConfig::Config1);

        case LightingSampler::Distribution1:
            return (config != LightingConfig::Config0) && (config != LightingConfig::Config1) &&
                   (config != LightingConfig::Config5);

        case LightingSampler::Fresnel:
            return (config != LightingConfig::Config0) && (config != LightingConfig::Config2) &&
                   (config != LightingConfig::Config4);

        case LightingSampler::ReflectRed:
            return (config != LightingConfig::Config3);

        case LightingSampler::ReflectGreen:
        case LightingSampler::ReflectBlue:
            return (config == LightingConfig::Config4) || (config == LightingConfig::Config5) ||
                   (config == LightingConfig::Config7);
        default:
            UNREACHABLE_MSG("Regs::IsLightingSamplerSupported: Reached "
                            "unreachable section, sampler should be one "
                            "of Distribution0, Distribution1, Fresnel, "
                            "ReflectRed, ReflectGreen or ReflectBlue, instead "
                            "got %i",
                            static_cast<int>(config));
        }
    }

    struct {
        struct LightSrc {
            LightColor specular_0; // material.specular_0 * light.specular_0
            LightColor specular_1; // material.specular_1 * light.specular_1
            LightColor diffuse;    // material.diffuse * light.diffuse
            LightColor ambient;    // material.ambient * light.ambient

            // Encoded as 16-bit floating point
            union {
                BitField<0, 16, u32> x;
                BitField<16, 16, u32> y;
            };
            union {
                BitField<0, 16, u32> z;
            };

            INSERT_PADDING_WORDS(0x3);

            union {
                BitField<0, 1, u32> directional;
                BitField<1, 1, u32> two_sided_diffuse; // When disabled, clamp dot-product to 0
            } config;

            BitField<0, 20, u32> dist_atten_bias;
            BitField<0, 20, u32> dist_atten_scale;

            INSERT_PADDING_WORDS(0x4);
        };
        static_assert(sizeof(LightSrc) == 0x10 * sizeof(u32),
                      "LightSrc structure must be 0x10 words");

        LightSrc light[8];
        LightColor global_ambient; // Emission + (material.ambient * lighting.ambient)
        INSERT_PADDING_WORDS(0x1);
        BitField<0, 3, u32> num_lights; // Number of enabled lights - 1

        union {
            BitField<2, 2, LightingFresnelSelector> fresnel_selector;
            BitField<4, 4, LightingConfig> config;
            BitField<22, 2, u32> bump_selector; // 0: Texture 0, 1: Texture 1, 2: Texture 2
            BitField<27, 1, u32> clamp_highlights;
            BitField<28, 2, LightingBumpMode> bump_mode;
            BitField<30, 1, u32> disable_bump_renorm;
        } config0;

        union {
            BitField<16, 1, u32> disable_lut_d0;
            BitField<17, 1, u32> disable_lut_d1;
            BitField<19, 1, u32> disable_lut_fr;
            BitField<20, 1, u32> disable_lut_rr;
            BitField<21, 1, u32> disable_lut_rg;
            BitField<22, 1, u32> disable_lut_rb;

            // Each bit specifies whether distance attenuation should be applied for the
            // corresponding light

            BitField<24, 1, u32> disable_dist_atten_light_0;
            BitField<25, 1, u32> disable_dist_atten_light_1;
            BitField<26, 1, u32> disable_dist_atten_light_2;
            BitField<27, 1, u32> disable_dist_atten_light_3;
            BitField<28, 1, u32> disable_dist_atten_light_4;
            BitField<29, 1, u32> disable_dist_atten_light_5;
            BitField<30, 1, u32> disable_dist_atten_light_6;
            BitField<31, 1, u32> disable_dist_atten_light_7;
        } config1;

        bool IsDistAttenDisabled(unsigned index) const {
            const unsigned disable[] = {
                config1.disable_dist_atten_light_0, config1.disable_dist_atten_light_1,
                config1.disable_dist_atten_light_2, config1.disable_dist_atten_light_3,
                config1.disable_dist_atten_light_4, config1.disable_dist_atten_light_5,
                config1.disable_dist_atten_light_6, config1.disable_dist_atten_light_7};
            return disable[index] != 0;
        }

        union {
            BitField<0, 8, u32> index; ///< Index at which to set data in the LUT
            BitField<8, 5, u32> type;  ///< Type of LUT for which to set data
        } lut_config;

        BitField<0, 1, u32> disable;
        INSERT_PADDING_WORDS(0x1);

        // When data is written to any of these registers, it gets written to the lookup table of
        // the selected type at the selected index, specified above in the `lut_config` register.
        // With each write, `lut_config.index` is incremented. It does not matter which of these
        // registers is written to, the behavior will be the same.
        u32 lut_data[8];

        // These are used to specify if absolute (abs) value should be used for each LUT index. When
        // abs mode is disabled, LUT indexes are in the range of (-1.0, 1.0). Otherwise, they are in
        // the range of (0.0, 1.0).
        union {
            BitField<1, 1, u32> disable_d0;
            BitField<5, 1, u32> disable_d1;
            BitField<9, 1, u32> disable_sp;
            BitField<13, 1, u32> disable_fr;
            BitField<17, 1, u32> disable_rb;
            BitField<21, 1, u32> disable_rg;
            BitField<25, 1, u32> disable_rr;
        } abs_lut_input;

        union {
            BitField<0, 3, LightingLutInput> d0;
            BitField<4, 3, LightingLutInput> d1;
            BitField<8, 3, LightingLutInput> sp;
            BitField<12, 3, LightingLutInput> fr;
            BitField<16, 3, LightingLutInput> rb;
            BitField<20, 3, LightingLutInput> rg;
            BitField<24, 3, LightingLutInput> rr;
        } lut_input;

        union {
            BitField<0, 3, LightingScale> d0;
            BitField<4, 3, LightingScale> d1;
            BitField<8, 3, LightingScale> sp;
            BitField<12, 3, LightingScale> fr;
            BitField<16, 3, LightingScale> rb;
            BitField<20, 3, LightingScale> rg;
            BitField<24, 3, LightingScale> rr;

            static float GetScale(LightingScale scale) {
                switch (scale) {
                case LightingScale::Scale1:
                    return 1.0f;
                case LightingScale::Scale2:
                    return 2.0f;
                case LightingScale::Scale4:
                    return 4.0f;
                case LightingScale::Scale8:
                    return 8.0f;
                case LightingScale::Scale1_4:
                    return 0.25f;
                case LightingScale::Scale1_2:
                    return 0.5f;
                }
                return 0.0f;
            }
        } lut_scale;

        INSERT_PADDING_WORDS(0x6);

        union {
            // There are 8 light enable "slots", corresponding to the total number of lights
            // supported by Pica. For N enabled lights (specified by register 0x1c2, or 'src_num'
            // above), the first N slots below will be set to integers within the range of 0-7,
            // corresponding to the actual light that is enabled for each slot.

            BitField<0, 3, u32> slot_0;
            BitField<4, 3, u32> slot_1;
            BitField<8, 3, u32> slot_2;
            BitField<12, 3, u32> slot_3;
            BitField<16, 3, u32> slot_4;
            BitField<20, 3, u32> slot_5;
            BitField<24, 3, u32> slot_6;
            BitField<28, 3, u32> slot_7;

            unsigned GetNum(unsigned index) const {
                const unsigned enable_slots[] = {slot_0, slot_1, slot_2, slot_3,
                                                 slot_4, slot_5, slot_6, slot_7};
                return enable_slots[index];
            }
        } light_enable;
    } lighting;

    INSERT_PADDING_WORDS(0x26);

    enum class VertexAttributeFormat : u64 {
        BYTE = 0,
        UBYTE = 1,
        SHORT = 2,
        FLOAT = 3,
    };

    struct {
        BitField<0, 29, u32> base_address;

        u32 GetPhysicalBaseAddress() const {
            return DecodeAddressRegister(base_address);
        }

        // Descriptor for internal vertex attributes
        union {
            BitField<0, 2, VertexAttributeFormat> format0; // size of one element
            BitField<2, 2, u64> size0;                     // number of elements minus 1
            BitField<4, 2, VertexAttributeFormat> format1;
            BitField<6, 2, u64> size1;
            BitField<8, 2, VertexAttributeFormat> format2;
            BitField<10, 2, u64> size2;
            BitField<12, 2, VertexAttributeFormat> format3;
            BitField<14, 2, u64> size3;
            BitField<16, 2, VertexAttributeFormat> format4;
            BitField<18, 2, u64> size4;
            BitField<20, 2, VertexAttributeFormat> format5;
            BitField<22, 2, u64> size5;
            BitField<24, 2, VertexAttributeFormat> format6;
            BitField<26, 2, u64> size6;
            BitField<28, 2, VertexAttributeFormat> format7;
            BitField<30, 2, u64> size7;
            BitField<32, 2, VertexAttributeFormat> format8;
            BitField<34, 2, u64> size8;
            BitField<36, 2, VertexAttributeFormat> format9;
            BitField<38, 2, u64> size9;
            BitField<40, 2, VertexAttributeFormat> format10;
            BitField<42, 2, u64> size10;
            BitField<44, 2, VertexAttributeFormat> format11;
            BitField<46, 2, u64> size11;

            BitField<48, 12, u64> attribute_mask;

            // number of total attributes minus 1
            BitField<60, 4, u64> num_extra_attributes;
        };

        inline VertexAttributeFormat GetFormat(int n) const {
            VertexAttributeFormat formats[] = {format0, format1, format2,  format3,
                                               format4, format5, format6,  format7,
                                               format8, format9, format10, format11};
            return formats[n];
        }

        inline int GetNumElements(int n) const {
            u64 sizes[] = {size0, size1, size2, size3, size4,  size5,
                           size6, size7, size8, size9, size10, size11};
            return (int)sizes[n] + 1;
        }

        inline int GetElementSizeInBytes(int n) const {
            return (GetFormat(n) == VertexAttributeFormat::FLOAT)
                       ? 4
                       : (GetFormat(n) == VertexAttributeFormat::SHORT) ? 2 : 1;
        }

        inline int GetStride(int n) const {
            return GetNumElements(n) * GetElementSizeInBytes(n);
        }

        inline bool IsDefaultAttribute(int id) const {
            return (id >= 12) || (attribute_mask & (1ULL << id)) != 0;
        }

        inline int GetNumTotalAttributes() const {
            return (int)num_extra_attributes + 1;
        }

        // Attribute loaders map the source vertex data to input attributes
        // This e.g. allows to load different attributes from different memory locations
        struct {
            // Source attribute data offset from the base address
            u32 data_offset;

            union {
                BitField<0, 4, u64> comp0;
                BitField<4, 4, u64> comp1;
                BitField<8, 4, u64> comp2;
                BitField<12, 4, u64> comp3;
                BitField<16, 4, u64> comp4;
                BitField<20, 4, u64> comp5;
                BitField<24, 4, u64> comp6;
                BitField<28, 4, u64> comp7;
                BitField<32, 4, u64> comp8;
                BitField<36, 4, u64> comp9;
                BitField<40, 4, u64> comp10;
                BitField<44, 4, u64> comp11;

                // bytes for a single vertex in this loader
                BitField<48, 8, u64> byte_count;

                BitField<60, 4, u64> component_count;
            };

            inline int GetComponent(int n) const {
                u64 components[] = {comp0, comp1, comp2, comp3, comp4,  comp5,
                                    comp6, comp7, comp8, comp9, comp10, comp11};
                return (int)components[n];
            }
        } attribute_loaders[12];
    } vertex_attributes;

    struct {
        enum IndexFormat : u32 {
            BYTE = 0,
            SHORT = 1,
        };

        union {
            BitField<0, 31, u32> offset; // relative to base attribute address
            BitField<31, 1, IndexFormat> format;
        };
    } index_array;

    // Number of vertices to render
    u32 num_vertices;

    INSERT_PADDING_WORDS(0x1);

    // The index of the first vertex to render
    u32 vertex_offset;

    INSERT_PADDING_WORDS(0x3);

    // These two trigger rendering of triangles
    u32 trigger_draw;
    u32 trigger_draw_indexed;

    INSERT_PADDING_WORDS(0x2);

    // These registers are used to setup the default "fall-back" vertex shader attributes
    struct {
        // Index of the current default attribute
        u32 index;

        // Writing to these registers sets the "current" default attribute.
        u32 set_value[3];
    } vs_default_attributes_setup;

    INSERT_PADDING_WORDS(0x2);

    struct {
        // There are two channels that can be used to configure the next command buffer, which
        // can be then executed by writing to the "trigger" registers. There are two reasons why a
        // game might use this feature:
        //  1) With this, an arbitrary number of additional command buffers may be executed in
        //     sequence without requiring any intervention of the CPU after the initial one is
        //     kicked off.
        //  2) Games can configure these registers to provide a command list subroutine mechanism.

        BitField<0, 20, u32> size[2]; ///< Size (in bytes / 8) of each channel's command buffer
        BitField<0, 28, u32> addr[2]; ///< Physical address / 8 of each channel's command buffer
        u32 trigger[2]; ///< Triggers execution of the channel's command buffer when written to

        unsigned GetSize(unsigned index) const {
            ASSERT(index < 2);
            return 8 * size[index];
        }

        PAddr GetPhysicalAddress(unsigned index) const {
            ASSERT(index < 2);
            return (PAddr)(8 * addr[index]);
        }
    } command_buffer;

    INSERT_PADDING_WORDS(0x07);

    enum class GPUMode : u32 {
        Drawing = 0,
        Configuring = 1,
    };

    GPUMode gpu_mode;

    INSERT_PADDING_WORDS(0x18);

    enum class TriangleTopology : u32 {
        List = 0,
        Strip = 1,
        Fan = 2,
        Shader = 3, // Programmable setup unit implemented in a geometry shader
    };

    BitField<8, 2, TriangleTopology> triangle_topology;

    u32 restart_primitive;

    INSERT_PADDING_WORDS(0x20);

    struct ShaderConfig {
        BitField<0, 16, u32> bool_uniforms;

        union {
            BitField<0, 8, u32> x;
            BitField<8, 8, u32> y;
            BitField<16, 8, u32> z;
            BitField<24, 8, u32> w;
        } int_uniforms[4];

        INSERT_PADDING_WORDS(0x4);

        union {
            // Number of input attributes to shader unit - 1
            BitField<0, 4, u32> num_input_attributes;
        };

        // Offset to shader program entry point (in words)
        BitField<0, 16, u32> main_offset;

        union {
            BitField<0, 4, u64> attribute0_register;
            BitField<4, 4, u64> attribute1_register;
            BitField<8, 4, u64> attribute2_register;
            BitField<12, 4, u64> attribute3_register;
            BitField<16, 4, u64> attribute4_register;
            BitField<20, 4, u64> attribute5_register;
            BitField<24, 4, u64> attribute6_register;
            BitField<28, 4, u64> attribute7_register;
            BitField<32, 4, u64> attribute8_register;
            BitField<36, 4, u64> attribute9_register;
            BitField<40, 4, u64> attribute10_register;
            BitField<44, 4, u64> attribute11_register;
            BitField<48, 4, u64> attribute12_register;
            BitField<52, 4, u64> attribute13_register;
            BitField<56, 4, u64> attribute14_register;
            BitField<60, 4, u64> attribute15_register;

            int GetRegisterForAttribute(int attribute_index) const {
                u64 fields[] = {
                    attribute0_register,  attribute1_register,  attribute2_register,
                    attribute3_register,  attribute4_register,  attribute5_register,
                    attribute6_register,  attribute7_register,  attribute8_register,
                    attribute9_register,  attribute10_register, attribute11_register,
                    attribute12_register, attribute13_register, attribute14_register,
                    attribute15_register,
                };
                return (int)fields[attribute_index];
            }
        } input_register_map;

        BitField<0, 16, u32> output_mask;

        // 0x28E, CODETRANSFER_END
        INSERT_PADDING_WORDS(0x2);

        struct {
            enum Format : u32 {
                FLOAT24 = 0,
                FLOAT32 = 1,
            };

            bool IsFloat32() const {
                return format == FLOAT32;
            }

            union {
                // Index of the next uniform to write to
                // TODO: ctrulib uses 8 bits for this, however that seems to yield lots of invalid
                // indices
                // TODO: Maybe the uppermost index is for the geometry shader? Investigate!
                BitField<0, 7, u32> index;

                BitField<31, 1, Format> format;
            };

            // Writing to these registers sets the current uniform.
            u32 set_value[8];

        } uniform_setup;

        INSERT_PADDING_WORDS(0x2);

        struct {
            // Offset of the next instruction to write code to.
            // Incremented with each instruction write.
            u32 offset;

            // Writing to these registers sets the "current" word in the shader program.
            u32 set_word[8];
        } program;

        INSERT_PADDING_WORDS(0x1);

        // This register group is used to load an internal table of swizzling patterns,
        // which are indexed by each shader instruction to specify vector component swizzling.
        struct {
            // Offset of the next swizzle pattern to write code to.
            // Incremented with each instruction write.
            u32 offset;

            // Writing to these registers sets the current swizzle pattern in the table.
            u32 set_word[8];
        } swizzle_patterns;

        INSERT_PADDING_WORDS(0x2);
    };

    ShaderConfig gs;
    ShaderConfig vs;

    INSERT_PADDING_WORDS(0x20);

    // Map register indices to names readable by humans
    // Used for debugging purposes, so performance is not an issue here
    static std::string GetCommandName(int index);

    static constexpr size_t NumIds() {
        return sizeof(Regs) / sizeof(u32);
    }

    const u32& operator[](int index) const {
        const u32* content = reinterpret_cast<const u32*>(this);
        return content[index];
    }

    u32& operator[](int index) {
        u32* content = reinterpret_cast<u32*>(this);
        return content[index];
    }

private:
    /*
     * Most physical addresses which Pica registers refer to are 8-byte aligned.
     * This function should be used to get the address from a raw register value.
     */
    static inline u32 DecodeAddressRegister(u32 register_value) {
        return register_value * 8;
    }
};

// TODO: MSVC does not support using offsetof() on non-static data members even though this
//       is technically allowed since C++11. This macro should be enabled once MSVC adds
//       support for that.
#ifndef _MSC_VER
#define ASSERT_REG_POSITION(field_name, position)                                                  \
    static_assert(offsetof(Regs, field_name) == position * 4,                                      \
                  "Field " #field_name " has invalid position")

ASSERT_REG_POSITION(trigger_irq, 0x10);
ASSERT_REG_POSITION(cull_mode, 0x40);
ASSERT_REG_POSITION(viewport_size_x, 0x41);
ASSERT_REG_POSITION(viewport_size_y, 0x43);
ASSERT_REG_POSITION(viewport_depth_range, 0x4d);
ASSERT_REG_POSITION(viewport_depth_near_plane, 0x4e);
ASSERT_REG_POSITION(vs_output_attributes[0], 0x50);
ASSERT_REG_POSITION(vs_output_attributes[1], 0x51);
ASSERT_REG_POSITION(scissor_test, 0x65);
ASSERT_REG_POSITION(viewport_corner, 0x68);
ASSERT_REG_POSITION(depthmap_enable, 0x6D);
ASSERT_REG_POSITION(texture0_enable, 0x80);
ASSERT_REG_POSITION(texture0, 0x81);
ASSERT_REG_POSITION(texture0_format, 0x8e);
ASSERT_REG_POSITION(fragment_lighting_enable, 0x8f);
ASSERT_REG_POSITION(texture1, 0x91);
ASSERT_REG_POSITION(texture1_format, 0x96);
ASSERT_REG_POSITION(texture2, 0x99);
ASSERT_REG_POSITION(texture2_format, 0x9e);
ASSERT_REG_POSITION(tev_stage0, 0xc0);
ASSERT_REG_POSITION(tev_stage1, 0xc8);
ASSERT_REG_POSITION(tev_stage2, 0xd0);
ASSERT_REG_POSITION(tev_stage3, 0xd8);
ASSERT_REG_POSITION(tev_combiner_buffer_input, 0xe0);
ASSERT_REG_POSITION(fog_mode, 0xe0);
ASSERT_REG_POSITION(fog_color, 0xe1);
ASSERT_REG_POSITION(fog_lut_offset, 0xe6);
ASSERT_REG_POSITION(fog_lut_data, 0xe8);
ASSERT_REG_POSITION(tev_stage4, 0xf0);
ASSERT_REG_POSITION(tev_stage5, 0xf8);
ASSERT_REG_POSITION(tev_combiner_buffer_color, 0xfd);
ASSERT_REG_POSITION(output_merger, 0x100);
ASSERT_REG_POSITION(framebuffer, 0x110);
ASSERT_REG_POSITION(lighting, 0x140);
ASSERT_REG_POSITION(vertex_attributes, 0x200);
ASSERT_REG_POSITION(index_array, 0x227);
ASSERT_REG_POSITION(num_vertices, 0x228);
ASSERT_REG_POSITION(vertex_offset, 0x22a);
ASSERT_REG_POSITION(trigger_draw, 0x22e);
ASSERT_REG_POSITION(trigger_draw_indexed, 0x22f);
ASSERT_REG_POSITION(vs_default_attributes_setup, 0x232);
ASSERT_REG_POSITION(command_buffer, 0x238);
ASSERT_REG_POSITION(gpu_mode, 0x245);
ASSERT_REG_POSITION(triangle_topology, 0x25e);
ASSERT_REG_POSITION(restart_primitive, 0x25f);
ASSERT_REG_POSITION(gs, 0x280);
ASSERT_REG_POSITION(vs, 0x2b0);

#undef ASSERT_REG_POSITION
#endif // !defined(_MSC_VER)

static_assert(sizeof(Regs::ShaderConfig) == 0x30 * sizeof(u32),
              "ShaderConfig structure has incorrect size");

// The total number of registers is chosen arbitrarily, but let's make sure it's not some odd value
// anyway.
static_assert(sizeof(Regs) <= 0x300 * sizeof(u32),
              "Register set structure larger than it should be");
static_assert(sizeof(Regs) >= 0x300 * sizeof(u32),
              "Register set structure smaller than it should be");

/// Initialize Pica state
void Init();

/// Shutdown Pica state
void Shutdown();

} // namespace
