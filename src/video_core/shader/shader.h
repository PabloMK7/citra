// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <vector>
#include <boost/container/static_vector.hpp>
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

    OutputVertex ToVertex(const Regs::ShaderConfig& config);
};
static_assert(std::is_pod<OutputRegisters>::value, "Structure is not POD");

// Helper structure used to keep track of data useful for inspection of shader emulation
template <bool full_debugging>
struct DebugData;

template <>
struct DebugData<false> {
    // TODO: Hide these behind and interface and move them to DebugData<true>
    u32 max_offset;    // maximum program counter ever reached
    u32 max_opdesc_id; // maximum swizzle pattern index ever used
};

template <>
struct DebugData<true> {
    // Records store the input and output operands of a particular instruction.
    struct Record {
        enum Type {
            // Floating point arithmetic operands
            SRC1 = 0x1,
            SRC2 = 0x2,
            SRC3 = 0x4,

            // Initial and final output operand value
            DEST_IN = 0x8,
            DEST_OUT = 0x10,

            // Current and next instruction offset (in words)
            CUR_INSTR = 0x20,
            NEXT_INSTR = 0x40,

            // Output address register value
            ADDR_REG_OUT = 0x80,

            // Result of a comparison instruction
            CMP_RESULT = 0x100,

            // Input values for conditional flow control instructions
            COND_BOOL_IN = 0x200,
            COND_CMP_IN = 0x400,

            // Input values for a loop
            LOOP_INT_IN = 0x800,
        };

        Math::Vec4<float24> src1;
        Math::Vec4<float24> src2;
        Math::Vec4<float24> src3;

        Math::Vec4<float24> dest_in;
        Math::Vec4<float24> dest_out;

        s32 address_registers[2];
        bool conditional_code[2];
        bool cond_bool;
        bool cond_cmp[2];
        Math::Vec4<u8> loop_int;

        u32 instruction_offset;
        u32 next_instruction;

        // set of enabled fields (as a combination of Type flags)
        unsigned mask = 0;
    };

    u32 max_offset;    // maximum program counter ever reached
    u32 max_opdesc_id; // maximum swizzle pattern index ever used

    // List of records for each executed shader instruction
    std::vector<DebugData<true>::Record> records;
};

// Type alias for better readability
using DebugDataRecord = DebugData<true>::Record;

// Helper function to set a DebugData<true>::Record field based on the template enum parameter.
template <DebugDataRecord::Type type, typename ValueType>
inline void SetField(DebugDataRecord& record, ValueType value);

template <>
inline void SetField<DebugDataRecord::SRC1>(DebugDataRecord& record, float24* value) {
    record.src1.x = value[0];
    record.src1.y = value[1];
    record.src1.z = value[2];
    record.src1.w = value[3];
}

template <>
inline void SetField<DebugDataRecord::SRC2>(DebugDataRecord& record, float24* value) {
    record.src2.x = value[0];
    record.src2.y = value[1];
    record.src2.z = value[2];
    record.src2.w = value[3];
}

template <>
inline void SetField<DebugDataRecord::SRC3>(DebugDataRecord& record, float24* value) {
    record.src3.x = value[0];
    record.src3.y = value[1];
    record.src3.z = value[2];
    record.src3.w = value[3];
}

template <>
inline void SetField<DebugDataRecord::DEST_IN>(DebugDataRecord& record, float24* value) {
    record.dest_in.x = value[0];
    record.dest_in.y = value[1];
    record.dest_in.z = value[2];
    record.dest_in.w = value[3];
}

template <>
inline void SetField<DebugDataRecord::DEST_OUT>(DebugDataRecord& record, float24* value) {
    record.dest_out.x = value[0];
    record.dest_out.y = value[1];
    record.dest_out.z = value[2];
    record.dest_out.w = value[3];
}

template <>
inline void SetField<DebugDataRecord::ADDR_REG_OUT>(DebugDataRecord& record, s32* value) {
    record.address_registers[0] = value[0];
    record.address_registers[1] = value[1];
}

template <>
inline void SetField<DebugDataRecord::CMP_RESULT>(DebugDataRecord& record, bool* value) {
    record.conditional_code[0] = value[0];
    record.conditional_code[1] = value[1];
}

template <>
inline void SetField<DebugDataRecord::COND_BOOL_IN>(DebugDataRecord& record, bool value) {
    record.cond_bool = value;
}

template <>
inline void SetField<DebugDataRecord::COND_CMP_IN>(DebugDataRecord& record, bool* value) {
    record.cond_cmp[0] = value[0];
    record.cond_cmp[1] = value[1];
}

template <>
inline void SetField<DebugDataRecord::LOOP_INT_IN>(DebugDataRecord& record, Math::Vec4<u8> value) {
    record.loop_int = value;
}

template <>
inline void SetField<DebugDataRecord::CUR_INSTR>(DebugDataRecord& record, u32 value) {
    record.instruction_offset = value;
}

template <>
inline void SetField<DebugDataRecord::NEXT_INSTR>(DebugDataRecord& record, u32 value) {
    record.next_instruction = value;
}

// Helper function to set debug information on the current shader iteration.
template <DebugDataRecord::Type type, typename ValueType>
inline void Record(DebugData<false>& debug_data, u32 offset, ValueType value) {
    // Debugging disabled => nothing to do
}

template <DebugDataRecord::Type type, typename ValueType>
inline void Record(DebugData<true>& debug_data, u32 offset, ValueType value) {
    if (offset >= debug_data.records.size())
        debug_data.records.resize(offset + 1);

    SetField<type, ValueType>(debug_data.records[offset], value);
    debug_data.records[offset].mask |= type;
}

/**
 * This structure contains the state information that needs to be unique for a shader unit. The 3DS
 * has four shader units that process shaders in parallel. At the present, Citra only implements a
 * single shader unit that processes all shaders serially. Putting the state information in a struct
 * here will make it easier for us to parallelize the shader processing later.
 */
template <bool Debug>
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

    DebugData<Debug> debug;

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
};

/// Clears the shader cache
void ClearCache();

struct ShaderSetup {

    struct {
        // The float uniforms are accessed by the shader JIT using SSE instructions, and are
        // therefore required to be 16-byte aligned.
        alignas(16) Math::Vec4<float24> f[96];

        std::array<bool, 16> b;
        std::array<Math::Vec4<u8>, 4> i;
    } uniforms;

    static size_t UniformOffset(RegisterType type, unsigned index) {
        switch (type) {
        case RegisterType::FloatUniform:
            return offsetof(ShaderSetup, uniforms.f) + index * sizeof(Math::Vec4<float24>);

        case RegisterType::BoolUniform:
            return offsetof(ShaderSetup, uniforms.b) + index * sizeof(bool);

        case RegisterType::IntUniform:
            return offsetof(ShaderSetup, uniforms.i) + index * sizeof(Math::Vec4<u8>);

        default:
            UNREACHABLE();
            return 0;
        }
    }

    std::array<u32, 1024> program_code;
    std::array<u32, 1024> swizzle_data;

    /**
     * Performs any shader unit setup that only needs to happen once per shader (as opposed to once
     * per vertex, which would happen within the `Run` function).
     */
    void Setup();

    /**
     * Runs the currently setup shader
     * @param state Shader unit state, must be setup per shader and per shader unit
     * @param input Input vertex into the shader
     * @param num_attributes The number of vertex shader attributes
     */
    void Run(UnitState<false>& state, const InputVertex& input, int num_attributes);

    /**
     * Produce debug information based on the given shader and input vertex
     * @param input Input vertex into the shader
     * @param num_attributes The number of vertex shader attributes
     * @param config Configuration object for the shader pipeline
     * @param setup Setup object for the shader pipeline
     * @return Debug information for this shader with regards to the given vertex
     */
    DebugData<true> ProduceDebugInfo(const InputVertex& input, int num_attributes,
                                     const Regs::ShaderConfig& config, const ShaderSetup& setup);
};

} // namespace Shader

} // namespace Pica
