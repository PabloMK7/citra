// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include <cstring>
#include "common/bit_set.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "video_core/pica_state.h"
#include "video_core/regs_rasterizer.h"
#include "video_core/regs_shader.h"
#include "video_core/shader/shader.h"
#include "video_core/shader/shader_interpreter.h"
#ifdef ARCHITECTURE_x86_64
#include "video_core/shader/shader_jit_x64.h"
#endif // ARCHITECTURE_x86_64
#include "video_core/video_core.h"

namespace Pica {

namespace Shader {

OutputVertex OutputVertex::FromAttributeBuffer(const RasterizerRegs& regs,
                                               const AttributeBuffer& input) {
    // Setup output data
    union {
        OutputVertex ret{};
        std::array<float24, 24> vertex_slots;
    };
    static_assert(sizeof(vertex_slots) == sizeof(ret), "Struct and array have different sizes.");

    unsigned int num_attributes = regs.vs_output_total;
    ASSERT(num_attributes <= 7);
    for (unsigned int i = 0; i < num_attributes; ++i) {
        const auto& output_register_map = regs.vs_output_attributes[i];

        RasterizerRegs::VSOutputAttributes::Semantic semantics[4] = {
            output_register_map.map_x, output_register_map.map_y, output_register_map.map_z,
            output_register_map.map_w};

        for (unsigned comp = 0; comp < 4; ++comp) {
            RasterizerRegs::VSOutputAttributes::Semantic semantic = semantics[comp];
            if (semantic < vertex_slots.size()) {
                vertex_slots[semantic] = input.attr[i][comp];
            } else if (semantic != RasterizerRegs::VSOutputAttributes::INVALID) {
                LOG_ERROR(HW_GPU, "Invalid/unknown semantic id: %u", (unsigned int)semantic);
            }
        }
    }

    // The hardware takes the absolute and saturates vertex colors like this, *before* doing
    // interpolation
    for (unsigned i = 0; i < 4; ++i) {
        float c = std::fabs(ret.color[i].ToFloat32());
        ret.color[i] = float24::FromFloat32(c < 1.0f ? c : 1.0f);
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

void UnitState::LoadInput(const ShaderRegs& config, const AttributeBuffer& input) {
    const unsigned max_attribute = config.max_input_attribute_index;

    for (unsigned attr = 0; attr <= max_attribute; ++attr) {
        unsigned reg = config.GetRegisterForAttribute(attr);
        registers.input[reg] = input.attr[attr];
    }
}

void UnitState::WriteOutput(const ShaderRegs& config, AttributeBuffer& output) {
    unsigned int output_i = 0;
    for (unsigned int reg : Common::BitSet<u32>(config.output_mask)) {
        output.attr[output_i++] = registers.output[reg];
    }
}

UnitState::UnitState(GSEmitter* emitter) : emitter_ptr(emitter) {}

GSEmitter::GSEmitter() {
    handlers = new Handlers;
}

GSEmitter::~GSEmitter() {
    delete handlers;
}

void GSEmitter::Emit(Math::Vec4<float24> (&vertex)[16]) {
    ASSERT(vertex_id < 3);
    std::copy(std::begin(vertex), std::end(vertex), buffer[vertex_id].begin());
    if (prim_emit) {
        if (winding)
            handlers->winding_setter();
        for (size_t i = 0; i < buffer.size(); ++i) {
            AttributeBuffer output;
            unsigned int output_i = 0;
            for (unsigned int reg : Common::BitSet<u32>(output_mask)) {
                output.attr[output_i++] = buffer[i][reg];
            }
            handlers->vertex_handler(output);
        }
    }
}

GSUnitState::GSUnitState() : UnitState(&emitter) {}

void GSUnitState::SetVertexHandler(VertexHandler vertex_handler, WindingSetter winding_setter) {
    emitter.handlers->vertex_handler = std::move(vertex_handler);
    emitter.handlers->winding_setter = std::move(winding_setter);
}

void GSUnitState::ConfigOutput(const ShaderRegs& config) {
    emitter.output_mask = config.output_mask;
}

MICROPROFILE_DEFINE(GPU_Shader, "GPU", "Shader", MP_RGB(50, 50, 240));

#ifdef ARCHITECTURE_x86_64
static std::unique_ptr<JitX64Engine> jit_engine;
#endif // ARCHITECTURE_x86_64
static InterpreterEngine interpreter_engine;

ShaderEngine* GetEngine() {
#ifdef ARCHITECTURE_x86_64
    // TODO(yuriks): Re-initialize on each change rather than being persistent
    if (VideoCore::g_shader_jit_enabled) {
        if (jit_engine == nullptr) {
            jit_engine = std::make_unique<JitX64Engine>();
        }
        return jit_engine.get();
    }
#endif // ARCHITECTURE_x86_64

    return &interpreter_engine;
}

void Shutdown() {
#ifdef ARCHITECTURE_x86_64
    jit_engine = nullptr;
#endif // ARCHITECTURE_x86_64
}

} // namespace Shader

} // namespace Pica
