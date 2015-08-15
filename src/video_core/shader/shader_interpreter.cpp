// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <common/file_util.h>

#include <nihstro/shader_bytecode.h>

#include "video_core/pica.h"

#include "shader.h"
#include "shader_interpreter.h"

using nihstro::OpCode;
using nihstro::Instruction;
using nihstro::RegisterType;
using nihstro::SourceRegister;
using nihstro::SwizzlePattern;

namespace Pica {

namespace Shader {

void RunInterpreter(UnitState& state) {
    const auto& uniforms = g_state.vs.uniforms;
    const auto& swizzle_data = g_state.vs.swizzle_data;
    const auto& program_code = g_state.vs.program_code;

    // Placeholder for invalid inputs
    static float24 dummy_vec4_float24[4];

    while (true) {
        if (!state.call_stack.empty()) {
            auto& top = state.call_stack.back();
            if (state.program_counter == top.final_address) {
                state.address_registers[2] += top.loop_increment;

                if (top.repeat_counter-- == 0) {
                    state.program_counter = top.return_address;
                    state.call_stack.pop_back();
                } else {
                    state.program_counter = top.loop_address;
                }

                // TODO: Is "trying again" accurate to hardware?
                continue;
            }
        }

        bool exit_loop = false;
        const Instruction instr = { program_code[state.program_counter] };
        const SwizzlePattern swizzle = { swizzle_data[instr.common.operand_desc_id] };

        static auto call = [](UnitState& state, u32 offset, u32 num_instructions,
                              u32 return_offset, u8 repeat_count, u8 loop_increment) {
            state.program_counter = offset - 1; // -1 to make sure when incrementing the PC we end up at the correct offset
            ASSERT(state.call_stack.size() < state.call_stack.capacity());
            state.call_stack.push_back({ offset + num_instructions, return_offset, repeat_count, loop_increment, offset });
        };
        state.debug.max_offset = std::max<u32>(state.debug.max_offset, 1 + state.program_counter);

        auto LookupSourceRegister = [&](const SourceRegister& source_reg) -> const float24* {
            switch (source_reg.GetRegisterType()) {
            case RegisterType::Input:
                return &state.registers.input[source_reg.GetIndex()].x;

            case RegisterType::Temporary:
                return &state.registers.temporary[source_reg.GetIndex()].x;

            case RegisterType::FloatUniform:
                return &uniforms.f[source_reg.GetIndex()].x;

            default:
                return dummy_vec4_float24;
            }
        };

        switch (instr.opcode.Value().GetInfo().type) {
        case OpCode::Type::Arithmetic:
        {
            const bool is_inverted = (0 != (instr.opcode.Value().GetInfo().subtype & OpCode::Info::SrcInversed));

            const int address_offset = (instr.common.address_register_index == 0)
                                       ? 0 : state.address_registers[instr.common.address_register_index - 1];

            const float24* src1_ = LookupSourceRegister(instr.common.GetSrc1(is_inverted) + (!is_inverted * address_offset));
            const float24* src2_ = LookupSourceRegister(instr.common.GetSrc2(is_inverted) + ( is_inverted * address_offset));

            const bool negate_src1 = ((bool)swizzle.negate_src1 != false);
            const bool negate_src2 = ((bool)swizzle.negate_src2 != false);

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

            float24* dest = (instr.common.dest.Value() < 0x10) ? &state.registers.output[instr.common.dest.Value().GetIndex()][0]
                        : (instr.common.dest.Value() < 0x20) ? &state.registers.temporary[instr.common.dest.Value().GetIndex()][0]
                        : dummy_vec4_float24;

            state.debug.max_opdesc_id = std::max<u32>(state.debug.max_opdesc_id, 1+instr.common.operand_desc_id);

            switch (instr.opcode.Value().EffectiveOpCode()) {
            case OpCode::Id::ADD:
            {
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = src1[i] + src2[i];
                }

                break;
            }

            case OpCode::Id::MUL:
            {
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = src1[i] * src2[i];
                }

                break;
            }

            case OpCode::Id::FLR:
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = float24::FromFloat32(std::floor(src1[i].ToFloat32()));
                }
                break;

            case OpCode::Id::MAX:
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = std::max(src1[i], src2[i]);
                }
                break;

            case OpCode::Id::MIN:
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = std::min(src1[i], src2[i]);
                }
                break;

            case OpCode::Id::DP3:
            case OpCode::Id::DP4:
            {
                float24 dot = float24::FromFloat32(0.f);
                int num_components = (instr.opcode.Value() == OpCode::Id::DP3) ? 3 : 4;
                for (int i = 0; i < num_components; ++i)
                    dot = dot + src1[i] * src2[i];

                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = dot;
                }
                break;
            }

            // Reciprocal
            case OpCode::Id::RCP:
            {
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    // TODO: Be stable against division by zero!
                    // TODO: I think this might be wrong... we should only use one component here
                    dest[i] = float24::FromFloat32(1.0f / src1[i].ToFloat32());
                }

                break;
            }

            // Reciprocal Square Root
            case OpCode::Id::RSQ:
            {
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    // TODO: Be stable against division by zero!
                    // TODO: I think this might be wrong... we should only use one component here
                    dest[i] = float24::FromFloat32(1.0f / sqrt(src1[i].ToFloat32()));
                }

                break;
            }

            case OpCode::Id::MOVA:
            {
                for (int i = 0; i < 2; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    // TODO: Figure out how the rounding is done on hardware
                    state.address_registers[i] = static_cast<s32>(src1[i].ToFloat32());
                }

                break;
            }

            case OpCode::Id::MOV:
            {
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = src1[i];
                }
                break;
            }

            case OpCode::Id::SLT:
            case OpCode::Id::SLTI:
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = (src1[i] < src2[i]) ? float24::FromFloat32(1.0f) : float24::FromFloat32(0.0f);
                }
                break;

            case OpCode::Id::CMP:
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
                          (int)instr.opcode.Value().EffectiveOpCode(), instr.opcode.Value().GetInfo().name, instr.hex);
                DEBUG_ASSERT(false);
                break;
            }

            break;
        }

        case OpCode::Type::MultiplyAdd:
        {
            if ((instr.opcode.Value().EffectiveOpCode() == OpCode::Id::MAD) ||
                (instr.opcode.Value().EffectiveOpCode() == OpCode::Id::MADI)) {
                const SwizzlePattern& swizzle = *(SwizzlePattern*)&swizzle_data[instr.mad.operand_desc_id];

                bool is_inverted = (instr.opcode.Value().EffectiveOpCode() == OpCode::Id::MADI);

                const float24* src1_ = LookupSourceRegister(instr.mad.GetSrc1(is_inverted));
                const float24* src2_ = LookupSourceRegister(instr.mad.GetSrc2(is_inverted));
                const float24* src3_ = LookupSourceRegister(instr.mad.GetSrc3(is_inverted));

                const bool negate_src1 = ((bool)swizzle.negate_src1 != false);
                const bool negate_src2 = ((bool)swizzle.negate_src2 != false);
                const bool negate_src3 = ((bool)swizzle.negate_src3 != false);

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
                float24 src3[4] = {
                    src3_[(int)swizzle.GetSelectorSrc3(0)],
                    src3_[(int)swizzle.GetSelectorSrc3(1)],
                    src3_[(int)swizzle.GetSelectorSrc3(2)],
                    src3_[(int)swizzle.GetSelectorSrc3(3)],
                };
                if (negate_src3) {
                    src3[0] = src3[0] * float24::FromFloat32(-1);
                    src3[1] = src3[1] * float24::FromFloat32(-1);
                    src3[2] = src3[2] * float24::FromFloat32(-1);
                    src3[3] = src3[3] * float24::FromFloat32(-1);
                }

                float24* dest = (instr.mad.dest.Value() < 0x10) ? &state.registers.output[instr.mad.dest.Value().GetIndex()][0]
                            : (instr.mad.dest.Value() < 0x20) ? &state.registers.temporary[instr.mad.dest.Value().GetIndex()][0]
                            : dummy_vec4_float24;

                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = src1[i] * src2[i] + src3[i];
                }
            } else {
                LOG_ERROR(HW_GPU, "Unhandled multiply-add instruction: 0x%02x (%s): 0x%08x",
                          (int)instr.opcode.Value().EffectiveOpCode(), instr.opcode.Value().GetInfo().name, instr.hex);
            }
            break;
        }

        default:
        {
            static auto evaluate_condition = [](const UnitState& state, bool refx, bool refy, Instruction::FlowControlType flow_control) {
                bool results[2] = { refx == state.conditional_code[0],
                                    refy == state.conditional_code[1] };

                switch (flow_control.op) {
                case flow_control.Or:
                    return results[0] || results[1];

                case flow_control.And:
                    return results[0] && results[1];

                case flow_control.JustX:
                    return results[0];

                case flow_control.JustY:
                    return results[1];
                }
            };

            // Handle each instruction on its own
            switch (instr.opcode.Value()) {
            case OpCode::Id::END:
                exit_loop = true;
                break;

            case OpCode::Id::JMPC:
                if (evaluate_condition(state, instr.flow_control.refx, instr.flow_control.refy, instr.flow_control)) {
                    state.program_counter = instr.flow_control.dest_offset - 1;
                }
                break;

            case OpCode::Id::JMPU:
                if (uniforms.b[instr.flow_control.bool_uniform_id]) {
                    state.program_counter = instr.flow_control.dest_offset - 1;
                }
                break;

            case OpCode::Id::CALL:
                call(state,
                     instr.flow_control.dest_offset,
                     instr.flow_control.num_instructions,
                     state.program_counter + 1, 0, 0);
                break;

            case OpCode::Id::CALLU:
                if (uniforms.b[instr.flow_control.bool_uniform_id]) {
                    call(state,
                        instr.flow_control.dest_offset,
                        instr.flow_control.num_instructions,
                        state.program_counter + 1, 0, 0);
                }
                break;

            case OpCode::Id::CALLC:
                if (evaluate_condition(state, instr.flow_control.refx, instr.flow_control.refy, instr.flow_control)) {
                    call(state,
                        instr.flow_control.dest_offset,
                        instr.flow_control.num_instructions,
                        state.program_counter + 1, 0, 0);
                }
                break;

            case OpCode::Id::NOP:
                break;

            case OpCode::Id::IFU:
                if (uniforms.b[instr.flow_control.bool_uniform_id]) {
                    call(state,
                         state.program_counter + 1,
                         instr.flow_control.dest_offset - state.program_counter - 1,
                         instr.flow_control.dest_offset + instr.flow_control.num_instructions, 0, 0);
                } else {
                    call(state,
                         instr.flow_control.dest_offset,
                         instr.flow_control.num_instructions,
                         instr.flow_control.dest_offset + instr.flow_control.num_instructions, 0, 0);
                }

                break;

            case OpCode::Id::IFC:
            {
                // TODO: Do we need to consider swizzlers here?

                if (evaluate_condition(state, instr.flow_control.refx, instr.flow_control.refy, instr.flow_control)) {
                    call(state,
                         state.program_counter + 1,
                         instr.flow_control.dest_offset - state.program_counter - 1,
                         instr.flow_control.dest_offset + instr.flow_control.num_instructions, 0, 0);
                } else {
                    call(state,
                         instr.flow_control.dest_offset,
                         instr.flow_control.num_instructions,
                         instr.flow_control.dest_offset + instr.flow_control.num_instructions, 0, 0);
                }

                break;
            }

            case OpCode::Id::LOOP:
            {
                state.address_registers[2] = uniforms.i[instr.flow_control.int_uniform_id].y;

                call(state,
                     state.program_counter + 1,
                     instr.flow_control.dest_offset - state.program_counter + 1,
                     instr.flow_control.dest_offset + 1,
                     uniforms.i[instr.flow_control.int_uniform_id].x,
                     uniforms.i[instr.flow_control.int_uniform_id].z);
                break;
            }

            default:
                LOG_ERROR(HW_GPU, "Unhandled instruction: 0x%02x (%s): 0x%08x",
                          (int)instr.opcode.Value().EffectiveOpCode(), instr.opcode.Value().GetInfo().name, instr.hex);
                break;
            }

            break;
        }
        }

        ++state.program_counter;

        if (exit_loop)
            break;
    }
}

} // namespace

} // namespace
