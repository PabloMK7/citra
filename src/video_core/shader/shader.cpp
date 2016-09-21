// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <boost/range/algorithm/fill.hpp>
#include "common/bit_field.h"
#include "common/hash.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "video_core/pica.h"
#include "video_core/pica_state.h"
#include "video_core/shader/shader.h"
#include "video_core/shader/shader_interpreter.h"
#ifdef ARCHITECTURE_x86_64
#include "video_core/shader/shader_jit_x64.h"
#endif // ARCHITECTURE_x86_64
#include "video_core/video_core.h"

namespace Pica {

namespace Shader {

OutputVertex OutputRegisters::ToVertex(const Regs::ShaderConfig& config) {
    // Setup output data
    OutputVertex ret;
    // TODO(neobrain): Under some circumstances, up to 16 attributes may be output. We need to
    // figure out what those circumstances are and enable the remaining outputs then.
    unsigned index = 0;
    for (unsigned i = 0; i < 7; ++i) {

        if (index >= g_state.regs.vs_output_total)
            break;

        if ((config.output_mask & (1 << i)) == 0)
            continue;

        const auto& output_register_map = g_state.regs.vs_output_attributes[index];

        u32 semantics[4] = {output_register_map.map_x, output_register_map.map_y,
                            output_register_map.map_z, output_register_map.map_w};

        for (unsigned comp = 0; comp < 4; ++comp) {
            float24* out = ((float24*)&ret) + semantics[comp];
            if (semantics[comp] != Regs::VSOutputAttributes::INVALID) {
                *out = value[i][comp];
            } else {
                // Zero output so that attributes which aren't output won't have denormals in them,
                // which would slow us down later.
                memset(out, 0, sizeof(*out));
            }
        }

        index++;
    }

    // The hardware takes the absolute and saturates vertex colors like this, *before* doing
    // interpolation
    for (unsigned i = 0; i < 4; ++i) {
        ret.color[i] = float24::FromFloat32(std::fmin(std::fabs(ret.color[i].ToFloat32()), 1.0f));
    }

    LOG_TRACE(HW_GPU, "Output vertex: pos(%.2f, %.2f, %.2f, %.2f), quat(%.2f, %.2f, %.2f, %.2f), "
                      "col(%.2f, %.2f, %.2f, %.2f), tc0(%.2f, %.2f), view(%.2f, %.2f, %.2f)",
              ret.pos.x.ToFloat32(), ret.pos.y.ToFloat32(), ret.pos.z.ToFloat32(),
              ret.pos.w.ToFloat32(), ret.quat.x.ToFloat32(), ret.quat.y.ToFloat32(),
              ret.quat.z.ToFloat32(), ret.quat.w.ToFloat32(), ret.color.x.ToFloat32(),
              ret.color.y.ToFloat32(), ret.color.z.ToFloat32(), ret.color.w.ToFloat32(),
              ret.tc0.u().ToFloat32(), ret.tc0.v().ToFloat32(), ret.view.x.ToFloat32(),
              ret.view.y.ToFloat32(), ret.view.z.ToFloat32());

    return ret;
}

#ifdef ARCHITECTURE_x86_64
static std::unordered_map<u64, std::unique_ptr<JitShader>> shader_map;
static const JitShader* jit_shader;
#endif // ARCHITECTURE_x86_64

void ClearCache() {
#ifdef ARCHITECTURE_x86_64
    shader_map.clear();
#endif // ARCHITECTURE_x86_64
}

void ShaderSetup::Setup() {
#ifdef ARCHITECTURE_x86_64
    if (VideoCore::g_shader_jit_enabled) {
        u64 cache_key =
            Common::ComputeHash64(&g_state.vs.program_code, sizeof(g_state.vs.program_code)) ^
            Common::ComputeHash64(&g_state.vs.swizzle_data, sizeof(g_state.vs.swizzle_data));

        auto iter = shader_map.find(cache_key);
        if (iter != shader_map.end()) {
            jit_shader = iter->second.get();
        } else {
            auto shader = std::make_unique<JitShader>();
            shader->Compile();
            jit_shader = shader.get();
            shader_map[cache_key] = std::move(shader);
        }
    }
#endif // ARCHITECTURE_x86_64
}

MICROPROFILE_DEFINE(GPU_Shader, "GPU", "Shader", MP_RGB(50, 50, 240));

void ShaderSetup::Run(UnitState<false>& state, const InputVertex& input, int num_attributes) {
    auto& config = g_state.regs.vs;
    auto& setup = g_state.vs;

    MICROPROFILE_SCOPE(GPU_Shader);

    state.debug.max_offset = 0;
    state.debug.max_opdesc_id = 0;

    // Setup input register table
    const auto& attribute_register_map = config.input_register_map;

    for (unsigned i = 0; i < num_attributes; i++)
        state.registers.input[attribute_register_map.GetRegisterForAttribute(i)] = input.attr[i];

    state.conditional_code[0] = false;
    state.conditional_code[1] = false;

#ifdef ARCHITECTURE_x86_64
    if (VideoCore::g_shader_jit_enabled)
        jit_shader->Run(setup, state, config.main_offset);
    else
        RunInterpreter(setup, state, config.main_offset);
#else
    RunInterpreter(setup, state, config.main_offset);
#endif // ARCHITECTURE_x86_64
}

DebugData<true> ShaderSetup::ProduceDebugInfo(const InputVertex& input, int num_attributes,
                                              const Regs::ShaderConfig& config,
                                              const ShaderSetup& setup) {
    UnitState<true> state;

    state.debug.max_offset = 0;
    state.debug.max_opdesc_id = 0;

    // Setup input register table
    const auto& attribute_register_map = config.input_register_map;
    float24 dummy_register;
    boost::fill(state.registers.input, &dummy_register);

    for (unsigned i = 0; i < num_attributes; i++)
        state.registers.input[attribute_register_map.GetRegisterForAttribute(i)] = input.attr[i];

    state.conditional_code[0] = false;
    state.conditional_code[1] = false;

    RunInterpreter(setup, state, config.main_offset);
    return state.debug;
}

} // namespace Shader

} // namespace Pica
