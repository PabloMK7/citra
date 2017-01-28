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
#include "video_core/regs_pipeline.h"
#include "video_core/regs_rasterizer.h"
#include "video_core/regs_shader.h"
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
    PipelineRegs pipeline;
    ShaderRegs gs;
    ShaderRegs vs;
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

ASSERT_REG_POSITION(pipeline, 0x200);
ASSERT_REG_POSITION(pipeline.vertex_attributes, 0x200);
ASSERT_REG_POSITION(pipeline.index_array, 0x227);
ASSERT_REG_POSITION(pipeline.num_vertices, 0x228);
ASSERT_REG_POSITION(pipeline.vertex_offset, 0x22a);
ASSERT_REG_POSITION(pipeline.trigger_draw, 0x22e);
ASSERT_REG_POSITION(pipeline.trigger_draw_indexed, 0x22f);
ASSERT_REG_POSITION(pipeline.vs_default_attributes_setup, 0x232);
ASSERT_REG_POSITION(pipeline.command_buffer, 0x238);
ASSERT_REG_POSITION(pipeline.gpu_mode, 0x245);
ASSERT_REG_POSITION(pipeline.triangle_topology, 0x25e);
ASSERT_REG_POSITION(pipeline.restart_primitive, 0x25f);

ASSERT_REG_POSITION(gs, 0x280);
ASSERT_REG_POSITION(vs, 0x2b0);

#undef ASSERT_REG_POSITION
#endif // !defined(_MSC_VER)

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
