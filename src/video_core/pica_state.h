// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/vector_math.h"
#include "video_core/geometry_pipeline.h"
#include "video_core/primitive_assembly.h"
#include "video_core/regs.h"
#include "video_core/shader/shader.h"

namespace Pica {

/// Struct used to describe current Pica state
struct State {
    State();
    void Reset();

    /// Pica registers
    Regs regs;

    Shader::ShaderSetup vs;
    Shader::ShaderSetup gs;

    Shader::AttributeBuffer input_default_attributes;

    struct ProcTex {
        union ValueEntry {
            u32 raw;

            // LUT value, encoded as 12-bit fixed point, with 12 fraction bits
            BitField<0, 12, u32> value; // 0.0.12 fixed point

            // Difference between two entry values. Used for efficient interpolation.
            // 0.0.12 fixed point with two's complement. The range is [-0.5, 0.5).
            // Note: the type of this is different from the one of lighting LUT
            BitField<12, 12, s32> difference;

            float ToFloat() const {
                return static_cast<float>(value) / 4095.f;
            }

            float DiffToFloat() const {
                return static_cast<float>(difference) / 4095.f;
            }
        };

        union ColorEntry {
            u32 raw;
            BitField<0, 8, u32> r;
            BitField<8, 8, u32> g;
            BitField<16, 8, u32> b;
            BitField<24, 8, u32> a;

            Common::Vec4<u8> ToVector() const {
                return {static_cast<u8>(r), static_cast<u8>(g), static_cast<u8>(b),
                        static_cast<u8>(a)};
            }
        };

        union ColorDifferenceEntry {
            u32 raw;
            BitField<0, 8, s32> r; // half of the difference between two ColorEntry
            BitField<8, 8, s32> g;
            BitField<16, 8, s32> b;
            BitField<24, 8, s32> a;

            Common::Vec4<s32> ToVector() const {
                return Common::Vec4<s32>{r, g, b, a} * 2;
            }
        };

        std::array<ValueEntry, 128> noise_table;
        std::array<ValueEntry, 128> color_map_table;
        std::array<ValueEntry, 128> alpha_map_table;
        std::array<ColorEntry, 256> color_table;
        std::array<ColorDifferenceEntry, 256> color_diff_table;
    } proctex;

    struct Lighting {
        union LutEntry {
            // Used for raw access
            u32 raw;

            // LUT value, encoded as 12-bit fixed point, with 12 fraction bits
            BitField<0, 12, u32> value; // 0.0.12 fixed point

            // Used for efficient interpolation.
            BitField<12, 11, u32> difference; // 0.0.11 fixed point
            BitField<23, 1, u32> neg_difference;

            float ToFloat() const {
                return static_cast<float>(value) / 4095.f;
            }

            float DiffToFloat() const {
                float diff = static_cast<float>(difference) / 2047.f;
                return neg_difference ? -diff : diff;
            }
        };

        std::array<std::array<LutEntry, 256>, 24> luts;
    } lighting;

    struct {
        union LutEntry {
            // Used for raw access
            u32 raw;

            BitField<0, 13, s32> difference; // 1.1.11 fixed point
            BitField<13, 11, u32> value;     // 0.0.11 fixed point

            float ToFloat() const {
                return static_cast<float>(value) / 2047.0f;
            }

            float DiffToFloat() const {
                return static_cast<float>(difference) / 2047.0f;
            }
        };

        std::array<LutEntry, 128> lut;
    } fog;

    /// Current Pica command list
    struct {
        const u32* head_ptr;
        const u32* current_ptr;
        u32 length;
    } cmd_list;

    /// Struct used to describe immediate mode rendering state
    struct ImmediateModeState {
        // Used to buffer partial vertices for immediate-mode rendering.
        Shader::AttributeBuffer input_vertex;
        // Index of the next attribute to be loaded into `input_vertex`.
        u32 current_attribute = 0;
        // Indicates the immediate mode just started and the geometry pipeline needs to reconfigure
        bool reset_geometry_pipeline = true;
    } immediate;

    // the geometry shader needs to be kept in the global state because some shaders relie on
    // preserved register value across shader invocation.
    // TODO: also bring the three vertex shader units here and implement the shader scheduler.
    Shader::GSUnitState gs_unit;

    GeometryPipeline geometry_pipeline;

    // This is constructed with a dummy triangle topology
    PrimitiveAssembler<Shader::OutputVertex> primitive_assembler;

    int vs_float_regs_counter = 0;
    u32 vs_uniform_write_buffer[4]{};

    int gs_float_regs_counter = 0;
    u32 gs_uniform_write_buffer[4]{};

    int default_attr_counter = 0;
    u32 default_attr_write_buffer[3]{};
};

extern State g_state; ///< Current Pica state

} // namespace Pica
