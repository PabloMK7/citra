// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <boost/range/algorithm.hpp>

#include <common/file_util.h>

#include <core/mem_map.h>

#include <nihstro/shader_bytecode.h>


#include "pica.h"
#include "vertex_shader.h"
#include "debug_utils/debug_utils.h"

using nihstro::Instruction;
using nihstro::RegisterType;
using nihstro::SourceRegister;
using nihstro::SwizzlePattern;

namespace Pica {

namespace VertexShader {

static struct {
    Math::Vec4<float24> f[96];

    std::array<bool,16> b;
} shader_uniforms;

// TODO: Not sure where the shader binary and swizzle patterns are supposed to be loaded to!
// For now, we just keep these local arrays around.
static std::array<u32, 1024> shader_memory;
static std::array<u32, 1024> swizzle_data;

void SubmitShaderMemoryChange(u32 addr, u32 value)
{
    shader_memory[addr] = value;
}

void SubmitSwizzleDataChange(u32 addr, u32 value)
{
    swizzle_data[addr] = value;
}

Math::Vec4<float24>& GetFloatUniform(u32 index)
{
    return shader_uniforms.f[index];
}

bool& GetBoolUniform(u32 index)
{
    return shader_uniforms.b[index];
}

const std::array<u32, 1024>& GetShaderBinary()
{
    return shader_memory;
}

const std::array<u32, 1024>& GetSwizzlePatterns()
{
    return swizzle_data;
}

// TODO: Is there actually a limit on hardware?
const int if_stack_size = 8;

struct VertexShaderState {
    u32* program_counter;

    const float24* input_register_table[16];
    float24* output_register_table[7*4];

    Math::Vec4<float24> temporary_registers[16];
    bool conditional_code[2];

    // Two Address registers and one loop counter
    // TODO: How many bits do these actually have?
    s32 address_registers[3];

    enum {
        INVALID_ADDRESS = 0xFFFFFFFF
    };
    u32 call_stack[8]; // TODO: What is the maximal call stack depth?
    u32* call_stack_pointer;

    struct IfStackElement {
        u32 else_addr;
        u32 else_instructions;
    } if_stack[if_stack_size];
    IfStackElement* if_stack_pointer;

    struct {
        u32 max_offset; // maximum program counter ever reached
        u32 max_opdesc_id; // maximum swizzle pattern index ever used
    } debug;
};

static void ProcessShaderCode(VertexShaderState& state) {
    while (true) {
        bool increment_pc = true;
        bool exit_loop = false;
        const Instruction& instr = *(const Instruction*)state.program_counter;
        const SwizzlePattern& swizzle = *(SwizzlePattern*)&swizzle_data[instr.common.operand_desc_id];

        state.debug.max_offset = std::max<u32>(state.debug.max_offset, 1 + (state.program_counter - shader_memory.data()));

        auto LookupSourceRegister = [&](const SourceRegister& source_reg) -> const float24* {
            switch (source_reg.GetRegisterType()) {
            case RegisterType::Input:
                return state.input_register_table[source_reg.GetIndex()];

            case RegisterType::Temporary:
                return &state.temporary_registers[source_reg.GetIndex()].x;

            case RegisterType::FloatUniform:
                return &shader_uniforms.f[source_reg.GetIndex()].x;
            }
        };

        switch (instr.opcode.GetInfo().type) {
        case Instruction::OpCodeType::Arithmetic:
        {
            bool is_inverted = 0 != (instr.opcode.GetInfo().subtype & Instruction::OpCodeInfo::SrcInversed);
            if (is_inverted) {
                // We don't really support this properly and/or reliably
                LOG_ERROR(HW_GPU, "Bad condition...");
                exit(0);
            }

            const int address_offset = (instr.common.address_register_index == 0)
                                       ? 0 : state.address_registers[instr.common.address_register_index - 1];

            const float24* src1_ = LookupSourceRegister(instr.common.GetSrc1(is_inverted) + address_offset);
            const float24* src2_ = LookupSourceRegister(instr.common.GetSrc2(is_inverted));

            const bool negate_src1 = (swizzle.negate_src1 != false);
            const bool negate_src2 = (swizzle.negate_src2 != false);

            float24 src1[4] = {
                src1_[(int)swizzle.GetSelectorSrc1(0)],
                src1_[(int)swizzle.GetSelectorSrc1(1)],
                src1_[(int)swizzle.GetSelectorSrc1(2)],
                src1_[(int)swizzle.GetSelectorSrc1(3)],
            };
            if (negate_src1) {
                src1[0] = src1[0] * float24::FromFloat32(-1);
                src1[1] = src1[1] * float24::FromFloat32(-1);
                src1[2] = src1[2] * float24::FromFloat32(-1);
                src1[3] = src1[3] * float24::FromFloat32(-1);
            }
            float24 src2[4] = {
                src2_[(int)swizzle.GetSelectorSrc2(0)],
                src2_[(int)swizzle.GetSelectorSrc2(1)],
                src2_[(int)swizzle.GetSelectorSrc2(2)],
                src2_[(int)swizzle.GetSelectorSrc2(3)],
            };
            if (negate_src2) {
                src2[0] = src2[0] * float24::FromFloat32(-1);
                src2[1] = src2[1] * float24::FromFloat32(-1);
                src2[2] = src2[2] * float24::FromFloat32(-1);
                src2[3] = src2[3] * float24::FromFloat32(-1);
            }

            float24* dest = (instr.common.dest < 0x08) ? state.output_register_table[4*instr.common.dest.GetIndex()]
                        : (instr.common.dest < 0x10) ? nullptr
                        : (instr.common.dest < 0x20) ? &state.temporary_registers[instr.common.dest.GetIndex()][0]
                        : nullptr;

            state.debug.max_opdesc_id = std::max<u32>(state.debug.max_opdesc_id, 1+instr.common.operand_desc_id);

            switch (instr.opcode.EffectiveOpCode()) {
            case Instruction::OpCode::ADD:
            {
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = src1[i] + src2[i];
                }

                break;
            }

            case Instruction::OpCode::MUL:
            {
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = src1[i] * src2[i];
                }

                break;
            }

            case Instruction::OpCode::MAX:
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = std::max(src1[i], src2[i]);
                }
                break;

            case Instruction::OpCode::DP3:
            case Instruction::OpCode::DP4:
            {
                float24 dot = float24::FromFloat32(0.f);
                int num_components = (instr.opcode == Instruction::OpCode::DP3) ? 3 : 4;
                for (int i = 0; i < num_components; ++i)
                    dot = dot + src1[i] * src2[i];

                for (int i = 0; i < num_components; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = dot;
                }
                break;
            }

            // Reciprocal
            case Instruction::OpCode::RCP:
            {
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    // TODO: Be stable against division by zero!
                    // TODO: I think this might be wrong... we should only use one component here
                    dest[i] = float24::FromFloat32(1.0 / src1[i].ToFloat32());
                }

                break;
            }

            // Reciprocal Square Root
            case Instruction::OpCode::RSQ:
            {
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    // TODO: Be stable against division by zero!
                    // TODO: I think this might be wrong... we should only use one component here
                    dest[i] = float24::FromFloat32(1.0 / sqrt(src1[i].ToFloat32()));
                }

                break;
            }

            case Instruction::OpCode::MOVA:
            {
                for (int i = 0; i < 2; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    // TODO: Figure out how the rounding is done on hardware
                    state.address_registers[i] = static_cast<s32>(src1[i].ToFloat32());
                }

                break;
            }

            case Instruction::OpCode::MOV:
            {
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = src1[i];
                }
                break;
            }

            case Instruction::OpCode::CMP:
                for (int i = 0; i < 2; ++i) {
                    // TODO: Can you restrict to one compare via dest masking?

                    auto compare_op = instr.common.compare_op;
                    auto op = (i == 0) ? compare_op.x.Value() : compare_op.y.Value();

                    switch (op) {
                        case compare_op.Equal:
                            state.conditional_code[i] = (src1[i] == src2[i]);
                            break;

                        case compare_op.NotEqual:
                            state.conditional_code[i] = (src1[i] != src2[i]);
                            break;

                        case compare_op.LessThan:
                            state.conditional_code[i] = (src1[i] <  src2[i]);
                            break;

                        case compare_op.LessEqual:
                            state.conditional_code[i] = (src1[i] <= src2[i]);
                            break;

                        case compare_op.GreaterThan:
                            state.conditional_code[i] = (src1[i] >  src2[i]);
                            break;

                        case compare_op.GreaterEqual:
                            state.conditional_code[i] = (src1[i] >= src2[i]);
                            break;

                        default:
                            LOG_ERROR(HW_GPU, "Unknown compare mode %x", static_cast<int>(op));
                            break;
                    }
                }
                break;

            default:
                LOG_ERROR(HW_GPU, "Unhandled arithmetic instruction: 0x%02x (%s): 0x%08x",
                          (int)instr.opcode.Value(), instr.opcode.GetInfo().name, instr.hex);
                _dbg_assert_(HW_GPU, 0);
                break;
            }

            break;
        }
        default:
            // Handle each instruction on its own
            switch (instr.opcode) {
            // NOP is currently used as a heuristic for leaving from a function.
            // TODO: This is completely incorrect.
            case Instruction::OpCode::NOP:
                if (*state.call_stack_pointer == VertexShaderState::INVALID_ADDRESS) {
                    exit_loop = true;
                } else {
                    // Jump back to call stack position, invalidate call stack entry, move up call stack pointer
                    state.program_counter = &shader_memory[*state.call_stack_pointer];
                    *state.call_stack_pointer-- = VertexShaderState::INVALID_ADDRESS;
                }

                break;

            case Instruction::OpCode::CALL:
                increment_pc = false;

                _dbg_assert_(HW_GPU, state.call_stack_pointer - state.call_stack < sizeof(state.call_stack));

                *++state.call_stack_pointer = state.program_counter - shader_memory.data();
                state.program_counter = &shader_memory[instr.flow_control.dest_offset];
                break;

            case Instruction::OpCode::END:
                // TODO
                break;

            case Instruction::OpCode::IFC:
            {
                // TODO: Do we need to consider swizzlers here?

                auto flow_control = instr.flow_control;
                bool results[3] = { flow_control.refx == state.conditional_code[0],
                                    flow_control.refy == state.conditional_code[1] };

                switch (flow_control.op) {
                case flow_control.Or:
                    results[2] = results[0] || results[1];
                    break;

                case flow_control.And:
                    results[2] = results[0] && results[1];
                    break;

                case flow_control.JustX:
                    results[2] = results[0];
                    break;

                case flow_control.JustY:
                    results[2] = results[1];
                    break;
                }

                if (results[2]) {
                    ++state.if_stack_pointer;

                    state.if_stack_pointer->else_addr = instr.flow_control.dest_offset;
                    state.if_stack_pointer->else_instructions = instr.flow_control.num_instructions;
                } else {
                    state.program_counter = &shader_memory[instr.flow_control.dest_offset] - 1;
                }

                break;
            }

            default:
                LOG_ERROR(HW_GPU, "Unhandled instruction: 0x%02x (%s): 0x%08x",
                          (int)instr.opcode.Value(), instr.opcode.GetInfo().name, instr.hex);
                break;
            }

            break;
        }

        if (increment_pc)
            ++state.program_counter;

        if (state.if_stack_pointer >= &state.if_stack[0]) {
            if (state.program_counter - shader_memory.data() == state.if_stack_pointer->else_addr) {
                state.program_counter += state.if_stack_pointer->else_instructions;
                state.if_stack_pointer--;
            }
        }

        if (exit_loop)
            break;
    }
}

OutputVertex RunShader(const InputVertex& input, int num_attributes)
{
    VertexShaderState state;

    const u32* main = &shader_memory[registers.vs_main_offset];
    state.program_counter = (u32*)main;
    state.debug.max_offset = 0;
    state.debug.max_opdesc_id = 0;

    // Setup input register table
    const auto& attribute_register_map = registers.vs_input_register_map;
    float24 dummy_register;
    boost::fill(state.input_register_table, &dummy_register);
    if(num_attributes > 0) state.input_register_table[attribute_register_map.attribute0_register] = &input.attr[0].x;
    if(num_attributes > 1) state.input_register_table[attribute_register_map.attribute1_register] = &input.attr[1].x;
    if(num_attributes > 2) state.input_register_table[attribute_register_map.attribute2_register] = &input.attr[2].x;
    if(num_attributes > 3) state.input_register_table[attribute_register_map.attribute3_register] = &input.attr[3].x;
    if(num_attributes > 4) state.input_register_table[attribute_register_map.attribute4_register] = &input.attr[4].x;
    if(num_attributes > 5) state.input_register_table[attribute_register_map.attribute5_register] = &input.attr[5].x;
    if(num_attributes > 6) state.input_register_table[attribute_register_map.attribute6_register] = &input.attr[6].x;
    if(num_attributes > 7) state.input_register_table[attribute_register_map.attribute7_register] = &input.attr[7].x;
    if(num_attributes > 8) state.input_register_table[attribute_register_map.attribute8_register] = &input.attr[8].x;
    if(num_attributes > 9) state.input_register_table[attribute_register_map.attribute9_register] = &input.attr[9].x;
    if(num_attributes > 10) state.input_register_table[attribute_register_map.attribute10_register] = &input.attr[10].x;
    if(num_attributes > 11) state.input_register_table[attribute_register_map.attribute11_register] = &input.attr[11].x;
    if(num_attributes > 12) state.input_register_table[attribute_register_map.attribute12_register] = &input.attr[12].x;
    if(num_attributes > 13) state.input_register_table[attribute_register_map.attribute13_register] = &input.attr[13].x;
    if(num_attributes > 14) state.input_register_table[attribute_register_map.attribute14_register] = &input.attr[14].x;
    if(num_attributes > 15) state.input_register_table[attribute_register_map.attribute15_register] = &input.attr[15].x;

    // Setup output register table
    OutputVertex ret;
    for (int i = 0; i < 7; ++i) {
        const auto& output_register_map = registers.vs_output_attributes[i];

        u32 semantics[4] = {
            output_register_map.map_x, output_register_map.map_y,
            output_register_map.map_z, output_register_map.map_w
        };

        for (int comp = 0; comp < 4; ++comp)
            state.output_register_table[4*i+comp] = ((float24*)&ret) + semantics[comp];
    }

    state.conditional_code[0] = false;
    state.conditional_code[1] = false;
    boost::fill(state.call_stack, VertexShaderState::INVALID_ADDRESS);
    state.call_stack_pointer = &state.call_stack[0];

    std::fill(state.if_stack, state.if_stack + sizeof(state.if_stack) / sizeof(state.if_stack[0]),
              VertexShaderState::IfStackElement{VertexShaderState::INVALID_ADDRESS, VertexShaderState::INVALID_ADDRESS});
    state.if_stack_pointer = state.if_stack - 1; // Meh. TODO: Make this less ugly

    ProcessShaderCode(state);
    DebugUtils::DumpShader(shader_memory.data(), state.debug.max_offset, swizzle_data.data(),
                           state.debug.max_opdesc_id, registers.vs_main_offset,
                           registers.vs_output_attributes);

    LOG_TRACE(Render_Software, "Output vertex: pos (%.2f, %.2f, %.2f, %.2f), col(%.2f, %.2f, %.2f, %.2f), tc0(%.2f, %.2f)",
        ret.pos.x.ToFloat32(), ret.pos.y.ToFloat32(), ret.pos.z.ToFloat32(), ret.pos.w.ToFloat32(),
        ret.color.x.ToFloat32(), ret.color.y.ToFloat32(), ret.color.z.ToFloat32(), ret.color.w.ToFloat32(),
        ret.tc0.u().ToFloat32(), ret.tc0.v().ToFloat32());

    return ret;
}


} // namespace

} // namespace
