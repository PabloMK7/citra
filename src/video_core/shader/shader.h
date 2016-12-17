// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <type_traits>
#include <nihstro/shader_bytecode.h>
#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/vector_math.h"
#include "video_core/pica.h"
#include "video_core/pica_types.h"

using nihstro::RegisterType;
using nihstro::SourceRegister;
using nihstro::DestRegister;

namespace Pica {

namespace Shader {

struct InputVertex {
    alignas(16) Math::Vec4<float24> attr[16];
};

struct OutputVertex {
    OutputVertex() = default;

    // VS output attributes
    Math::Vec4<float24> pos;
    Math::Vec4<float24> quat;
    Math::Vec4<float24> color;
    Math::Vec2<float24> tc0;
    Math::Vec2<float24> tc1;
    float24 tc0_w;
    INSERT_PADDING_WORDS(1);
    Math::Vec3<float24> view;
    INSERT_PADDING_WORDS(1);
    Math::Vec2<float24> tc2;

    // Padding for optimal alignment
    INSERT_PADDING_WORDS(4);

    // Attributes used to store intermediate results

    // position after perspective divide
    Math::Vec3<float24> screenpos;
    INSERT_PADDING_WORDS(1);

    // Linear interpolation
    // factor: 0=this, 1=vtx
    void Lerp(float24 factor, const OutputVertex& vtx) {
        pos = pos * factor + vtx.pos * (float24::FromFloat32(1) - factor);

        // TODO: Should perform perspective correct interpolation here...
        tc0 = tc0 * factor + vtx.tc0 * (float24::FromFloat32(1) - factor);
        tc1 = tc1 * factor + vtx.tc1 * (float24::FromFloat32(1) - factor);
        tc2 = tc2 * factor + vtx.tc2 * (float24::FromFloat32(1) - factor);

        screenpos = screenpos * factor + vtx.screenpos * (float24::FromFloat32(1) - factor);

        color = color * factor + vtx.color * (float24::FromFloat32(1) - factor);
    }

    // Linear interpolation
    // factor: 0=v0, 1=v1
    static OutputVertex Lerp(float24 factor, const OutputVertex& v0, const OutputVertex& v1) {
        OutputVertex ret = v0;
        ret.Lerp(factor, v1);
        return ret;
    }
};
static_assert(std::is_pod<OutputVertex>::value, "Structure is not POD");
static_assert(sizeof(OutputVertex) == 32 * sizeof(float), "OutputVertex has invalid size");

struct OutputRegisters {
    OutputRegisters() = default;

    alignas(16) Math::Vec4<float24> value[16];

    OutputVertex ToVertex(const Regs::ShaderConfig& config) const;
};
static_assert(std::is_pod<OutputRegisters>::value, "Structure is not POD");

/**
 * This structure contains the state information that needs to be unique for a shader unit. The 3DS
 * has four shader units that process shaders in parallel. At the present, Citra only implements a
 * single shader unit that processes all shaders serially. Putting the state information in a struct
 * here will make it easier for us to parallelize the shader processing later.
 */
struct UnitState {
    struct Registers {
        // The registers are accessed by the shader JIT using SSE instructions, and are therefore
        // required to be 16-byte aligned.
        alignas(16) Math::Vec4<float24> input[16];
        alignas(16) Math::Vec4<float24> temporary[16];
    } registers;
    static_assert(std::is_pod<Registers>::value, "Structure is not POD");

    OutputRegisters output_registers;

    bool conditional_code[2];

    // Two Address registers and one loop counter
    // TODO: How many bits do these actually have?
    s32 address_registers[3];

    static size_t InputOffset(const SourceRegister& reg) {
        switch (reg.GetRegisterType()) {
        case RegisterType::Input:
            return offsetof(UnitState, registers.input) +
                   reg.GetIndex() * sizeof(Math::Vec4<float24>);

        case RegisterType::Temporary:
            return offsetof(UnitState, registers.temporary) +
                   reg.GetIndex() * sizeof(Math::Vec4<float24>);

        default:
            UNREACHABLE();
            return 0;
        }
    }

    static size_t OutputOffset(const DestRegister& reg) {
        switch (reg.GetRegisterType()) {
        case RegisterType::Output:
            return offsetof(UnitState, output_registers.value) +
                   reg.GetIndex() * sizeof(Math::Vec4<float24>);

        case RegisterType::Temporary:
            return offsetof(UnitState, registers.temporary) +
                   reg.GetIndex() * sizeof(Math::Vec4<float24>);

        default:
            UNREACHABLE();
            return 0;
        }
    }

    /**
     * Loads the unit state with an input vertex.
     *
     * @param input Input vertex into the shader
     * @param num_attributes The number of vertex shader attributes to load
     */
    void LoadInputVertex(const InputVertex& input, int num_attributes);
};

struct ShaderSetup {
    struct {
        // The float uniforms are accessed by the shader JIT using SSE instructions, and are
        // therefore required to be 16-byte aligned.
        alignas(16) Math::Vec4<float24> f[96];

        std::array<bool, 16> b;
        std::array<Math::Vec4<u8>, 4> i;
    } uniforms;

    static size_t GetFloatUniformOffset(unsigned index) {
        return offsetof(ShaderSetup, uniforms.f) + index * sizeof(Math::Vec4<float24>);
    }

    static size_t GetBoolUniformOffset(unsigned index) {
        return offsetof(ShaderSetup, uniforms.b) + index * sizeof(bool);
    }

    static size_t GetIntUniformOffset(unsigned index) {
        return offsetof(ShaderSetup, uniforms.i) + index * sizeof(Math::Vec4<u8>);
    }

    std::array<u32, 1024> program_code;
    std::array<u32, 1024> swizzle_data;
};

class ShaderEngine {
public:
    virtual ~ShaderEngine() = default;

    /**
     * Performs any shader unit setup that only needs to happen once per shader (as opposed to once
     * per vertex, which would happen within the `Run` function).
     */
    virtual void SetupBatch(const ShaderSetup* setup) = 0;

    /**
     * Runs the currently setup shader
     * @param state Shader unit state, must be setup per shader and per shader unit
     */
    virtual void Run(UnitState& state, unsigned int entry_point) const = 0;
};

// TODO(yuriks): Remove and make it non-global state somewhere
ShaderEngine* GetEngine();
void Shutdown();

} // namespace Shader

} // namespace Pica
