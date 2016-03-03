// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/pica.h"
#include "video_core/primitive_assembly.h"
#include "video_core/shader/shader.h"

namespace Pica {

/// Struct used to describe current Pica state
struct State {
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
        Shader::InputVertex input;
        // This is constructed with a dummy triangle topology
        PrimitiveAssembler<Shader::OutputVertex> primitive_assembler;
        int attribute_id = 0;

        ImmediateModeState() : primitive_assembler(Regs::TriangleTopology::List) {}
    } immediate;
};

extern State g_state; ///< Current Pica state

} // namespace
