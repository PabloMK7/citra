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

struct PipelineRegs {
    enum class VertexAttributeFormat : u64 {
        BYTE = 0,
        UBYTE = 1,
        SHORT = 2,
        FLOAT = 3,
    };

    struct {
        BitField<0, 29, u32> base_address;

        PAddr GetPhysicalBaseAddress() const {
            return base_address * 8;
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
        // There are two channels that can be used to configure the next command buffer, which can
        // be then executed by writing to the "trigger" registers. There are two reasons why a game
        // might use this feature:
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
};

static_assert(sizeof(PipelineRegs) == 0x80 * sizeof(u32), "PipelineRegs struct has incorrect size");

} // namespace Pica
