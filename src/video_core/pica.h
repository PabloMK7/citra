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
#include "video_core/regs_framebuffer.h"
#include "video_core/regs_lighting.h"
#include "video_core/regs_rasterizer.h"
#include "video_core/regs_texturing.h"

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
    RasterizerRegs rasterizer;
    TexturingRegs texturing;
    FramebufferRegs framebuffer;
    LightingRegs lighting;

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
            BitField<60, 4, u64> max_attribute_index;
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
            return (int)max_attribute_index + 1;
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

    INSERT_PADDING_WORDS(4);

    /// Number of input attributes to the vertex shader minus 1
    BitField<0, 4, u32> max_input_attrib_index;

    INSERT_PADDING_WORDS(2);

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
            BitField<0, 4, u32> max_input_attribute_index;
        };

        // Offset to shader program entry point (in words)
        BitField<0, 16, u32> main_offset;

        /// Maps input attributes to registers. 4-bits per attribute, specifying a register index
        u32 input_attribute_to_register_map_low;
        u32 input_attribute_to_register_map_high;

        unsigned int GetRegisterForAttribute(unsigned int attribute_index) const {
            u64 map = ((u64)input_attribute_to_register_map_high << 32) |
                      (u64)input_attribute_to_register_map_low;
            return (map >> (attribute_index * 4)) & 0b1111;
        }

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

ASSERT_REG_POSITION(rasterizer, 0x40);
ASSERT_REG_POSITION(rasterizer.cull_mode, 0x40);
ASSERT_REG_POSITION(rasterizer.viewport_size_x, 0x41);
ASSERT_REG_POSITION(rasterizer.viewport_size_y, 0x43);
ASSERT_REG_POSITION(rasterizer.viewport_depth_range, 0x4d);
ASSERT_REG_POSITION(rasterizer.viewport_depth_near_plane, 0x4e);
ASSERT_REG_POSITION(rasterizer.vs_output_attributes[0], 0x50);
ASSERT_REG_POSITION(rasterizer.vs_output_attributes[1], 0x51);
ASSERT_REG_POSITION(rasterizer.scissor_test, 0x65);
ASSERT_REG_POSITION(rasterizer.viewport_corner, 0x68);
ASSERT_REG_POSITION(rasterizer.depthmap_enable, 0x6D);

ASSERT_REG_POSITION(texturing, 0x80);
ASSERT_REG_POSITION(texturing.texture0_enable, 0x80);
ASSERT_REG_POSITION(texturing.texture0, 0x81);
ASSERT_REG_POSITION(texturing.texture0_format, 0x8e);
ASSERT_REG_POSITION(texturing.fragment_lighting_enable, 0x8f);
ASSERT_REG_POSITION(texturing.texture1, 0x91);
ASSERT_REG_POSITION(texturing.texture1_format, 0x96);
ASSERT_REG_POSITION(texturing.texture2, 0x99);
ASSERT_REG_POSITION(texturing.texture2_format, 0x9e);
ASSERT_REG_POSITION(texturing.tev_stage0, 0xc0);
ASSERT_REG_POSITION(texturing.tev_stage1, 0xc8);
ASSERT_REG_POSITION(texturing.tev_stage2, 0xd0);
ASSERT_REG_POSITION(texturing.tev_stage3, 0xd8);
ASSERT_REG_POSITION(texturing.tev_combiner_buffer_input, 0xe0);
ASSERT_REG_POSITION(texturing.fog_mode, 0xe0);
ASSERT_REG_POSITION(texturing.fog_color, 0xe1);
ASSERT_REG_POSITION(texturing.fog_lut_offset, 0xe6);
ASSERT_REG_POSITION(texturing.fog_lut_data, 0xe8);
ASSERT_REG_POSITION(texturing.tev_stage4, 0xf0);
ASSERT_REG_POSITION(texturing.tev_stage5, 0xf8);
ASSERT_REG_POSITION(texturing.tev_combiner_buffer_color, 0xfd);

ASSERT_REG_POSITION(framebuffer, 0x100);
ASSERT_REG_POSITION(framebuffer.output_merger, 0x100);
ASSERT_REG_POSITION(framebuffer.framebuffer, 0x110);

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
