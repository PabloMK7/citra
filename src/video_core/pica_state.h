// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "common/bit_field.h"
#include "common/common_types.h"

#include "video_core/pica.h"
#include "video_core/primitive_assembly.h"
#include "video_core/shader/shader.h"

namespace Pica {

/// Struct used to describe current Pica state
struct State {
    void Reset();

    /// Pica registers
    Regs regs;

    Shader::ShaderSetup vs;
    Shader::ShaderSetup gs;

    struct {
        union LutEntry {
            // Used for raw access
            u32 raw;

            // LUT value, encoded as 12-bit fixed point, with 12 fraction bits
            BitField< 0, 12, u32> value;

            // Used by HW for efficient interpolation, Citra does not use these
            BitField<12, 12, u32> difference;

            float ToFloat() {
                return static_cast<float>(value) / 4095.f;
            }
        };

        std::array<std::array<LutEntry, 256>, 24> luts;
    } lighting;

    /// Current Pica command list
    struct {
        const u32* head_ptr;
        const u32* current_ptr;
        u32 length;
    } cmd_list;

    /// Struct used to describe immediate mode rendering state
    struct ImmediateModeState {
        // Used to buffer partial vertices for immediate-mode rendering.
        Shader::InputVertex input_vertex;
        // Index of the next attribute to be loaded into `input_vertex`.
        int current_attribute = 0;
    } immediate;

    // This is constructed with a dummy triangle topology
    PrimitiveAssembler<Shader::OutputVertex> primitive_assembler;
};

extern State g_state; ///< Current Pica state

} // namespace
