// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <string>

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
//       For details cf. https://connect.microsoft.com/VisualStudio/feedback/details/209229/offsetof-does-not-produce-a-constant-expression-for-array-members
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
#define PICA_REG_INDEX_WORKAROUND(field_name, backup_workaround_index) \
    ((typename std::enable_if<backup_workaround_index == PICA_REG_INDEX(field_name), size_t>::type)PICA_REG_INDEX(field_name))
#endif // _MSC_VER

struct Regs {

    INSERT_PADDING_WORDS(0x10);

    u32 trigger_irq;

    INSERT_PADDING_WORDS(0x2f);

    enum class CullMode : u32 {
        // Select which polygons are considered to be "frontfacing".
        KeepAll              = 0,
        KeepClockWise        = 1,
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

    BitField<0, 24, u32> viewport_depth_range; // float24
    BitField<0, 24, u32> viewport_depth_far_plane; // float24

    INSERT_PADDING_WORDS(0x1);

    union VSOutputAttributes {
        // Maps components of output vertex attributes to semantics
        enum Semantic : u32
        {
            POSITION_X   =  0,
            POSITION_Y   =  1,
            POSITION_Z   =  2,
            POSITION_W   =  3,

            COLOR_R      =  8,
            COLOR_G      =  9,
            COLOR_B      = 10,
            COLOR_A      = 11,

            TEXCOORD0_U  = 12,
            TEXCOORD0_V  = 13,
            TEXCOORD1_U  = 14,
            TEXCOORD1_V  = 15,
            TEXCOORD2_U  = 22,
            TEXCOORD2_V  = 23,

            INVALID      = 31,
        };

        BitField< 0, 5, Semantic> map_x;
        BitField< 8, 5, Semantic> map_y;
        BitField<16, 5, Semantic> map_z;
        BitField<24, 5, Semantic> map_w;
    } vs_output_attributes[7];

    INSERT_PADDING_WORDS(0x11);

    union {
        BitField< 0, 16, u32> x;
        BitField<16, 16, u32> y;
    } viewport_corner;

    INSERT_PADDING_WORDS(0x17);

    struct TextureConfig {
        enum WrapMode : u32 {
            ClampToEdge    = 0,
            ClampToBorder  = 1,
            Repeat         = 2,
            MirroredRepeat = 3,
        };

        enum TextureFilter : u32 {
            Nearest = 0,
            Linear  = 1
        };

        union {
            BitField< 0, 8, u32> r;
            BitField< 8, 8, u32> g;
            BitField<16, 8, u32> b;
            BitField<24, 8, u32> a;
        } border_color;

        union {
            BitField< 0, 16, u32> height;
            BitField<16, 16, u32> width;
        };

        union {
            BitField< 1, 1, TextureFilter> mag_filter;
            BitField< 2, 1, TextureFilter> min_filter;
            BitField< 8, 2, WrapMode> wrap_t;
            BitField<12, 2, WrapMode> wrap_s;
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
        RGBA8        =  0,
        RGB8         =  1,
        RGB5A1       =  2,
        RGB565       =  3,
        RGBA4        =  4,
        IA8          =  5,

        I8           =  7,
        A8           =  8,
        IA4          =  9,
        I4           = 10,
        A4           = 11,
        ETC1         = 12,  // compressed
        ETC1A4       = 13,  // compressed
    };

    enum class LogicOp : u32 {
        Clear        =  0,
        And          =  1,
        AndReverse   =  2,
        Copy         =  3,
        Set          =  4,
        CopyInverted =  5,
        NoOp         =  6,
        Invert       =  7,
        Nand         =  8,
        Or           =  9,
        Nor          = 10,
        Xor          = 11,
        Equiv        = 12,
        AndInverted  = 13,
        OrReverse    = 14,
        OrInverted   = 15,
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
            return 4;

        case TextureFormat::I4:
        case TextureFormat::A4:
            return 1;

        case TextureFormat::I8:
        case TextureFormat::A8:
        case TextureFormat::IA4:
        default:  // placeholder for yet unknown formats
            return 2;
        }
    }

    union {
        BitField< 0, 1, u32> texture0_enable;
        BitField< 1, 1, u32> texture1_enable;
        BitField< 2, 1, u32> texture2_enable;
    };
    TextureConfig texture0;
    INSERT_PADDING_WORDS(0x8);
    BitField<0, 4, TextureFormat> texture0_format;
    INSERT_PADDING_WORDS(0x2);
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
                   { texture0_enable.ToBool(), texture0, texture0_format },
                   { texture1_enable.ToBool(), texture1, texture1_format },
                   { texture2_enable.ToBool(), texture2, texture2_format }
               }};
    }

    // 0xc0-0xff: Texture Combiner (akin to glTexEnv)
    struct TevStageConfig {
        enum class Source : u32 {
            PrimaryColor           = 0x0,
            PrimaryFragmentColor   = 0x1,
            SecondaryFragmentColor = 0x2,

            Texture0               = 0x3,
            Texture1               = 0x4,
            Texture2               = 0x5,
            Texture3               = 0x6,

            PreviousBuffer         = 0xd,
            Constant               = 0xe,
            Previous               = 0xf,
        };

        enum class ColorModifier : u32 {
            SourceColor         = 0x0,
            OneMinusSourceColor = 0x1,
            SourceAlpha         = 0x2,
            OneMinusSourceAlpha = 0x3,
            SourceRed           = 0x4,
            OneMinusSourceRed   = 0x5,

            SourceGreen         = 0x8,
            OneMinusSourceGreen = 0x9,

            SourceBlue          = 0xc,
            OneMinusSourceBlue  = 0xd,
        };

        enum class AlphaModifier : u32 {
            SourceAlpha         = 0x0,
            OneMinusSourceAlpha = 0x1,
            SourceRed           = 0x2,
            OneMinusSourceRed   = 0x3,
            SourceGreen         = 0x4,
            OneMinusSourceGreen = 0x5,
            SourceBlue          = 0x6,
            OneMinusSourceBlue  = 0x7,
        };

        enum class Operation : u32 {
            Replace         = 0,
            Modulate        = 1,
            Add             = 2,
            AddSigned       = 3,
            Lerp            = 4,
            Subtract        = 5,
            Dot3_RGB        = 6,

            MultiplyThenAdd = 8,
            AddThenMultiply = 9,
        };

        union {
            BitField< 0, 4, Source> color_source1;
            BitField< 4, 4, Source> color_source2;
            BitField< 8, 4, Source> color_source3;
            BitField<16, 4, Source> alpha_source1;
            BitField<20, 4, Source> alpha_source2;
            BitField<24, 4, Source> alpha_source3;
        };

        union {
            BitField< 0, 4, ColorModifier> color_modifier1;
            BitField< 4, 4, ColorModifier> color_modifier2;
            BitField< 8, 4, ColorModifier> color_modifier3;
            BitField<12, 3, AlphaModifier> alpha_modifier1;
            BitField<16, 3, AlphaModifier> alpha_modifier2;
            BitField<20, 3, AlphaModifier> alpha_modifier3;
        };

        union {
            BitField< 0, 4, Operation> color_op;
            BitField<16, 4, Operation> alpha_op;
        };

        union {
            BitField< 0, 8, u32> const_r;
            BitField< 8, 8, u32> const_g;
            BitField<16, 8, u32> const_b;
            BitField<24, 8, u32> const_a;
        };

        union {
            BitField< 0, 2, u32> color_scale;
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

    union {
        // Tev stages 0-3 write their output to the combiner buffer if the corresponding bit in
        // these masks are set
        BitField< 8, 4, u32> update_mask_rgb;
        BitField<12, 4, u32> update_mask_a;

        bool TevStageUpdatesCombinerBufferColor(unsigned stage_index) const {
            return (stage_index < 4) && (update_mask_rgb & (1 << stage_index));
        }

        bool TevStageUpdatesCombinerBufferAlpha(unsigned stage_index) const {
            return (stage_index < 4) && (update_mask_a & (1 << stage_index));
        }
    } tev_combiner_buffer_input;

    INSERT_PADDING_WORDS(0xf);
    TevStageConfig tev_stage4;
    INSERT_PADDING_WORDS(0x3);
    TevStageConfig tev_stage5;

    union {
        BitField< 0, 8, u32> r;
        BitField< 8, 8, u32> g;
        BitField<16, 8, u32> b;
        BitField<24, 8, u32> a;
    } tev_combiner_buffer_color;

    INSERT_PADDING_WORDS(0x2);

    const std::array<Regs::TevStageConfig,6> GetTevStages() const {
        return {{ tev_stage0, tev_stage1,
                  tev_stage2, tev_stage3,
                  tev_stage4, tev_stage5 }};
    };

    enum class BlendEquation : u32 {
        Add             = 0,
        Subtract        = 1,
        ReverseSubtract = 2,
        Min             = 3,
        Max             = 4,
    };

    enum class BlendFactor : u32 {
        Zero                    = 0,
        One                     = 1,
        SourceColor             = 2,
        OneMinusSourceColor     = 3,
        DestColor               = 4,
        OneMinusDestColor       = 5,
        SourceAlpha             = 6,
        OneMinusSourceAlpha     = 7,
        DestAlpha               = 8,
        OneMinusDestAlpha       = 9,
        ConstantColor           = 10,
        OneMinusConstantColor   = 11,
        ConstantAlpha           = 12,
        OneMinusConstantAlpha   = 13,
        SourceAlphaSaturate     = 14,
    };

    enum class CompareFunc : u32 {
        Never              = 0,
        Always             = 1,
        Equal              = 2,
        NotEqual           = 3,
        LessThan           = 4,
        LessThanOrEqual    = 5,
        GreaterThan        = 6,
        GreaterThanOrEqual = 7,
    };

    enum class StencilAction : u32 {
        Keep = 0,
        Xor  = 5,
    };

    struct {
        union {
            // If false, logic blending is used
            BitField<8, 1, u32> alphablend_enable;
        };

        union {
            BitField< 0, 8, BlendEquation> blend_equation_rgb;
            BitField< 8, 8, BlendEquation> blend_equation_a;

            BitField<16, 4, BlendFactor> factor_source_rgb;
            BitField<20, 4, BlendFactor> factor_dest_rgb;

            BitField<24, 4, BlendFactor> factor_source_a;
            BitField<28, 4, BlendFactor> factor_dest_a;
        } alpha_blending;

        union {
            BitField<0, 4, LogicOp> logic_op;
        };

        union {
            BitField< 0, 8, u32> r;
            BitField< 8, 8, u32> g;
            BitField<16, 8, u32> b;
            BitField<24, 8, u32> a;
        } blend_const;

        union {
            BitField< 0, 1, u32> enable;
            BitField< 4, 3, CompareFunc> func;
            BitField< 8, 8, u32> ref;
        } alpha_test;

        struct {
            union {
                // If true, enable stencil testing
                BitField< 0, 1, u32> enable;

                // Comparison operation for stencil testing
                BitField< 4, 3, CompareFunc> func;

                // Value to calculate the new stencil value from
                BitField< 8, 8, u32> replacement_value;

                // Value to compare against for stencil testing
                BitField<16, 8, u32> reference_value;

                // Mask to apply on stencil test inputs
                BitField<24, 8, u32> mask;
            };

            union {
                // Action to perform when the stencil test fails
                BitField< 0, 3, StencilAction> action_stencil_fail;

                // Action to perform when stencil testing passed but depth testing fails
                BitField< 4, 3, StencilAction> action_depth_fail;

                // Action to perform when both stencil and depth testing pass
                BitField< 8, 3, StencilAction> action_depth_pass;
            };
        } stencil_test;

        union {
            BitField< 0, 1, u32> depth_test_enable;
            BitField< 4, 3, CompareFunc> depth_test_func;
            BitField< 8, 1, u32> red_enable;
            BitField< 9, 1, u32> green_enable;
            BitField<10, 1, u32> blue_enable;
            BitField<11, 1, u32> alpha_enable;
            BitField<12, 1, u32> depth_write_enable;
        };

        INSERT_PADDING_WORDS(0x8);
    } output_merger;

    // Components are laid out in reverse byte order, most significant bits first.
    enum class ColorFormat : u32 {
        RGBA8  = 0,
        RGB8   = 1,
        RGB5A1 = 2,
        RGB565 = 3,
        RGBA4  = 4,
    };

    enum class DepthFormat : u32 {
        D16   = 0,
        D24   = 2,
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

    struct {
        INSERT_PADDING_WORDS(0x6);

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
            BitField< 0, 11, u32> width;
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

    INSERT_PADDING_WORDS(0xe0);

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
            BitField< 0,  2, VertexAttributeFormat> format0; // size of one element
            BitField< 2,  2, u64> size0;      // number of elements minus 1
            BitField< 4,  2, VertexAttributeFormat> format1;
            BitField< 6,  2, u64> size1;
            BitField< 8,  2, VertexAttributeFormat> format2;
            BitField<10,  2, u64> size2;
            BitField<12,  2, VertexAttributeFormat> format3;
            BitField<14,  2, u64> size3;
            BitField<16,  2, VertexAttributeFormat> format4;
            BitField<18,  2, u64> size4;
            BitField<20,  2, VertexAttributeFormat> format5;
            BitField<22,  2, u64> size5;
            BitField<24,  2, VertexAttributeFormat> format6;
            BitField<26,  2, u64> size6;
            BitField<28,  2, VertexAttributeFormat> format7;
            BitField<30,  2, u64> size7;
            BitField<32,  2, VertexAttributeFormat> format8;
            BitField<34,  2, u64> size8;
            BitField<36,  2, VertexAttributeFormat> format9;
            BitField<38,  2, u64> size9;
            BitField<40,  2, VertexAttributeFormat> format10;
            BitField<42,  2, u64> size10;
            BitField<44,  2, VertexAttributeFormat> format11;
            BitField<46,  2, u64> size11;

            BitField<48, 12, u64> attribute_mask;

            // number of total attributes minus 1
            BitField<60,  4, u64> num_extra_attributes;
        };

        inline VertexAttributeFormat GetFormat(int n) const {
            VertexAttributeFormat formats[] = {
                format0, format1, format2, format3,
                format4, format5, format6, format7,
                format8, format9, format10, format11
            };
            return formats[n];
        }

        inline int GetNumElements(int n) const {
            u64 sizes[] = {
                size0, size1, size2, size3,
                size4, size5, size6, size7,
                size8, size9, size10, size11
            };
            return (int)sizes[n]+1;
        }

        inline int GetElementSizeInBytes(int n) const {
            return (GetFormat(n) == VertexAttributeFormat::FLOAT) ? 4 :
                (GetFormat(n) == VertexAttributeFormat::SHORT) ? 2 : 1;
        }

        inline int GetStride(int n) const {
            return GetNumElements(n) * GetElementSizeInBytes(n);
        }

        inline bool IsDefaultAttribute(int id) const {
            return (id >= 12) || (attribute_mask & (1ULL << id)) != 0;
        }

        inline int GetNumTotalAttributes() const {
            return (int)num_extra_attributes+1;
        }

        // Attribute loaders map the source vertex data to input attributes
        // This e.g. allows to load different attributes from different memory locations
        struct {
            // Source attribute data offset from the base address
            u32 data_offset;

            union {
                BitField< 0, 4, u64> comp0;
                BitField< 4, 4, u64> comp1;
                BitField< 8, 4, u64> comp2;
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
                u64 components[] = {
                    comp0, comp1, comp2, comp3,
                    comp4, comp5, comp6, comp7,
                    comp8, comp9, comp10, comp11
                };
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

    INSERT_PADDING_WORDS(0x5);

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

        BitField< 0, 20, u32> size[2]; ///< Size (in bytes / 8) of each channel's command buffer
        BitField< 0, 28, u32> addr[2]; ///< Physical address / 8 of each channel's command buffer
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

    INSERT_PADDING_WORDS(0x20);

    enum class TriangleTopology : u32 {
        List   = 0,
        Strip  = 1,
        Fan    = 2,
        Shader = 3, // Programmable setup unit implemented in a geometry shader
    };

    BitField<8, 2, TriangleTopology> triangle_topology;

    INSERT_PADDING_WORDS(0x21);

    struct ShaderConfig {
        BitField<0, 16, u32> bool_uniforms;

        union {
            BitField< 0, 8, u32> x;
            BitField< 8, 8, u32> y;
            BitField<16, 8, u32> z;
            BitField<24, 8, u32> w;
        } int_uniforms[4];

        INSERT_PADDING_WORDS(0x5);

        // Offset to shader program entry point (in words)
        BitField<0, 16, u32> main_offset;

        union {
            BitField< 0, 4, u64> attribute0_register;
            BitField< 4, 4, u64> attribute1_register;
            BitField< 8, 4, u64> attribute2_register;
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
                    attribute0_register,  attribute1_register,  attribute2_register,  attribute3_register,
                    attribute4_register,  attribute5_register,  attribute6_register,  attribute7_register,
                    attribute8_register,  attribute9_register,  attribute10_register, attribute11_register,
                    attribute12_register, attribute13_register, attribute14_register, attribute15_register,
                };
                return (int)fields[attribute_index];
            }
        } input_register_map;

        // OUTMAP_MASK, 0x28E, CODETRANSFER_END
        INSERT_PADDING_WORDS(0x3);

        struct {
            enum Format : u32
            {
                FLOAT24 = 0,
                FLOAT32 = 1
            };

            bool IsFloat32() const {
                return format == FLOAT32;
            }

            union {
                // Index of the next uniform to write to
                // TODO: ctrulib uses 8 bits for this, however that seems to yield lots of invalid indices
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

    static inline size_t NumIds() {
        return sizeof(Regs) / sizeof(u32);
    }

    u32& operator [] (int index) const {
        u32* content = (u32*)this;
        return content[index];
    }

    u32& operator [] (int index) {
        u32* content = (u32*)this;
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
#define ASSERT_REG_POSITION(field_name, position) static_assert(offsetof(Regs, field_name) == position * 4, "Field "#field_name" has invalid position")

ASSERT_REG_POSITION(trigger_irq, 0x10);
ASSERT_REG_POSITION(cull_mode, 0x40);
ASSERT_REG_POSITION(viewport_size_x, 0x41);
ASSERT_REG_POSITION(viewport_size_y, 0x43);
ASSERT_REG_POSITION(viewport_depth_range, 0x4d);
ASSERT_REG_POSITION(viewport_depth_far_plane, 0x4e);
ASSERT_REG_POSITION(vs_output_attributes[0], 0x50);
ASSERT_REG_POSITION(vs_output_attributes[1], 0x51);
ASSERT_REG_POSITION(viewport_corner, 0x68);
ASSERT_REG_POSITION(texture0_enable, 0x80);
ASSERT_REG_POSITION(texture0, 0x81);
ASSERT_REG_POSITION(texture0_format, 0x8e);
ASSERT_REG_POSITION(texture1, 0x91);
ASSERT_REG_POSITION(texture1_format, 0x96);
ASSERT_REG_POSITION(texture2, 0x99);
ASSERT_REG_POSITION(texture2_format, 0x9e);
ASSERT_REG_POSITION(tev_stage0, 0xc0);
ASSERT_REG_POSITION(tev_stage1, 0xc8);
ASSERT_REG_POSITION(tev_stage2, 0xd0);
ASSERT_REG_POSITION(tev_stage3, 0xd8);
ASSERT_REG_POSITION(tev_combiner_buffer_input, 0xe0);
ASSERT_REG_POSITION(tev_stage4, 0xf0);
ASSERT_REG_POSITION(tev_stage5, 0xf8);
ASSERT_REG_POSITION(tev_combiner_buffer_color, 0xfd);
ASSERT_REG_POSITION(output_merger, 0x100);
ASSERT_REG_POSITION(framebuffer, 0x110);
ASSERT_REG_POSITION(vertex_attributes, 0x200);
ASSERT_REG_POSITION(index_array, 0x227);
ASSERT_REG_POSITION(num_vertices, 0x228);
ASSERT_REG_POSITION(trigger_draw, 0x22e);
ASSERT_REG_POSITION(trigger_draw_indexed, 0x22f);
ASSERT_REG_POSITION(vs_default_attributes_setup, 0x232);
ASSERT_REG_POSITION(command_buffer, 0x238);
ASSERT_REG_POSITION(triangle_topology, 0x25e);
ASSERT_REG_POSITION(gs, 0x280);
ASSERT_REG_POSITION(vs, 0x2b0);

#undef ASSERT_REG_POSITION
#endif // !defined(_MSC_VER)

static_assert(sizeof(Regs::ShaderConfig) == 0x30 * sizeof(u32), "ShaderConfig structure has incorrect size");

// The total number of registers is chosen arbitrarily, but let's make sure it's not some odd value anyway.
static_assert(sizeof(Regs) <= 0x300 * sizeof(u32), "Register set structure larger than it should be");
static_assert(sizeof(Regs) >= 0x300 * sizeof(u32), "Register set structure smaller than it should be");

struct float24 {
    static float24 FromFloat32(float val) {
        float24 ret;
        ret.value = val;
        return ret;
    }

    // 16 bit mantissa, 7 bit exponent, 1 bit sign
    // TODO: No idea if this works as intended
    static float24 FromRawFloat24(u32 hex) {
        float24 ret;
        if ((hex & 0xFFFFFF) == 0) {
            ret.value = 0;
        } else {
            u32 mantissa = hex & 0xFFFF;
            u32 exponent = (hex >> 16) & 0x7F;
            u32 sign = hex >> 23;
            ret.value = std::pow(2.0f, (float)exponent-63.0f) * (1.0f + mantissa * std::pow(2.0f, -16.f));
            if (sign)
                ret.value = -ret.value;
        }
        return ret;
    }

    // Not recommended for anything but logging
    float ToFloat32() const {
        return value;
    }

    float24 operator * (const float24& flt) const {
        return float24::FromFloat32(ToFloat32() * flt.ToFloat32());
    }

    float24 operator / (const float24& flt) const {
        return float24::FromFloat32(ToFloat32() / flt.ToFloat32());
    }

    float24 operator + (const float24& flt) const {
        return float24::FromFloat32(ToFloat32() + flt.ToFloat32());
    }

    float24 operator - (const float24& flt) const {
        return float24::FromFloat32(ToFloat32() - flt.ToFloat32());
    }

    float24& operator *= (const float24& flt) {
        value *= flt.ToFloat32();
        return *this;
    }

    float24& operator /= (const float24& flt) {
        value /= flt.ToFloat32();
        return *this;
    }

    float24& operator += (const float24& flt) {
        value += flt.ToFloat32();
        return *this;
    }

    float24& operator -= (const float24& flt) {
        value -= flt.ToFloat32();
        return *this;
    }

    float24 operator - () const {
        return float24::FromFloat32(-ToFloat32());
    }

    bool operator < (const float24& flt) const {
        return ToFloat32() < flt.ToFloat32();
    }

    bool operator > (const float24& flt) const {
        return ToFloat32() > flt.ToFloat32();
    }

    bool operator >= (const float24& flt) const {
        return ToFloat32() >= flt.ToFloat32();
    }

    bool operator <= (const float24& flt) const {
        return ToFloat32() <= flt.ToFloat32();
    }

    bool operator == (const float24& flt) const {
        return ToFloat32() == flt.ToFloat32();
    }

    bool operator != (const float24& flt) const {
        return ToFloat32() != flt.ToFloat32();
    }

private:
    // Stored as a regular float, merely for convenience
    // TODO: Perform proper arithmetic on this!
    float value;
};

/// Struct used to describe current Pica state
struct State {
    /// Pica registers
    Regs regs;

    /// Vertex shader memory
    struct ShaderSetup {
        struct {
            Math::Vec4<float24> f[96];
            std::array<bool, 16> b;
            std::array<Math::Vec4<u8>, 4> i;
        } uniforms;

        Math::Vec4<float24> default_attributes[16];

        std::array<u32, 1024> program_code;
        std::array<u32, 1024> swizzle_data;
    };

    ShaderSetup vs;
    ShaderSetup gs;

    /// Current Pica command list
    struct {
        const u32* head_ptr;
        const u32* current_ptr;
        u32 length;
    } cmd_list;
};

/// Initialize Pica state
void Init();

/// Shutdown Pica state
void Shutdown();

extern State g_state; ///< Current Pica state

} // namespace
