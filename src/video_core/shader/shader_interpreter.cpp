// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

#include <nihstro/shader_bytecode.h>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/vector_math.h"

#include "video_core/pica_state.h"
#include "video_core/pica_types.h"
#include "video_core/shader/shader.h"
#include "video_core/shader/shader_interpreter.h"

using nihstro::OpCode;
using nihstro::Instruction;
using nihstro::RegisterType;
using nihstro::SourceRegister;
using nihstro::SwizzlePattern;

namespace Pica {

namespace Shader {

constexpr u32 INVALID_ADDRESS = 0xFFFFFFFF;

struct CallStackElement {
    u32 final_address;  // Address upon which we jump to return_address
    u32 return_address; // Where to jump when leaving scope
    u8 repeat_counter;  // How often to repeat until this call stack element is removed
    u8 loop_increment;  // Which value to add to the loop counter after an iteration
                        // TODO: Should this be a signed value? Does it even matter?
    u32 loop_address;   // The address where we'll return to after each loop iteration
};

template<bool Debug>
void RunInterpreter(const ShaderSetup& setup, UnitState<Debug>& state, unsigned offset) {
    // TODO: Is there a maximal size for this?
    boost::container::static_vector<CallStackElement, 16> call_stack;

    u32 program_counter = offset;

    const auto& uniforms = g_state.vs.uniforms;
    const auto& swizzle_data = g_state.vs.swizzle_data;
    const auto& program_code = g_state.vs.program_code;

    // Placeholder for invalid inputs
    static float24 dummy_vec4_float24[4];

    unsigned iteration = 0;
    bool exit_loop = false;
    while (!exit_loop) {
        if (!call_stack.empty()) {
            auto& top = call_stack.back();
            if (program_counter == top.final_address) {
                state.address_registers[2] += top.loop_increment;

                if (top.repeat_counter-- == 0) {
                    program_counter = top.return_address;
                    call_stack.pop_back();
                } else {
                    program_counter = top.loop_address;
                }

                // TODO: Is "trying again" accurate to hardware?
                continue;
            }
        }

        const Instruction instr = { program_code[program_counter] };
        const SwizzlePattern swizzle = { swizzle_data[instr.common.operand_desc_id] };

        auto call = [&program_counter, &call_stack](UnitState<Debug>& state, u32 offset, u32 num_instructions,
                              u32 return_offset, u8 repeat_count, u8 loop_increment) {
            program_counter = offset - 1; // -1 to make sure when incrementing the PC we end up at the correct offset
            ASSERT(call_stack.size() < call_stack.capacity());
            call_stack.push_back({ offset + num_instructions, return_offset, repeat_count, loop_increment, offset });
        };
        Record<DebugDataRecord::CUR_INSTR>(state.debug, iteration, program_counter);
        if (iteration > 0)
            Record<DebugDataRecord::NEXT_INSTR>(state.debug, iteration - 1, program_counter);

        state.debug.max_offset = std::max<u32>(state.debug.max_offset, 1 + program_counter);

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

            float24* dest = (instr.common.dest.Value() < 0x10) ? &state.output_registers.value[instr.common.dest.Value().GetIndex()][0]
                        : (instr.common.dest.Value() < 0x20) ? &state.registers.temporary[instr.common.dest.Value().GetIndex()][0]
                        : dummy_vec4_float24;

            state.debug.max_opdesc_id = std::max<u32>(state.debug.max_opdesc_id, 1+instr.common.operand_desc_id);

            switch (instr.opcode.Value().EffectiveOpCode()) {
            case OpCode::Id::ADD:
            {
                Record<DebugDataRecord::SRC1>(state.debug, iteration, src1);
                Record<DebugDataRecord::SRC2>(state.debug, iteration, src2);
                Record<DebugDataRecord::DEST_IN>(state.debug, iteration, dest);
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = src1[i] + src2[i];
                }
                Record<DebugDataRecord::DEST_OUT>(state.debug, iteration, dest);
                break;
            }

            case OpCode::Id::MUL:
            {
                Record<DebugDataRecord::SRC1>(state.debug, iteration, src1);
                Record<DebugDataRecord::SRC2>(state.debug, iteration, src2);
                Record<DebugDataRecord::DEST_IN>(state.debug, iteration, dest);
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = src1[i] * src2[i];
                }
                Record<DebugDataRecord::DEST_OUT>(state.debug, iteration, dest);
                break;
            }

            case OpCode::Id::FLR:
                Record<DebugDataRecord::SRC1>(state.debug, iteration, src1);
                Record<DebugDataRecord::DEST_IN>(state.debug, iteration, dest);
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = float24::FromFloat32(std::floor(src1[i].ToFloat32()));
                }
                Record<DebugDataRecord::DEST_OUT>(state.debug, iteration, dest);
                break;

            case OpCode::Id::MAX:
                Record<DebugDataRecord::SRC1>(state.debug, iteration, src1);
                Record<DebugDataRecord::SRC2>(state.debug, iteration, src2);
                Record<DebugDataRecord::DEST_IN>(state.debug, iteration, dest);
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    // NOTE: Exact form required to match NaN semantics to hardware:
                    //   max(0, NaN) -> NaN
                    //   max(NaN, 0) -> 0
                    dest[i] = (src1[i] > src2[i]) ? src1[i] : src2[i];
                }
                Record<DebugDataRecord::DEST_OUT>(state.debug, iteration, dest);
                break;

            case OpCode::Id::MIN:
                Record<DebugDataRecord::SRC1>(state.debug, iteration, src1);
                Record<DebugDataRecord::SRC2>(state.debug, iteration, src2);
                Record<DebugDataRecord::DEST_IN>(state.debug, iteration, dest);
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    // NOTE: Exact form required to match NaN semantics to hardware:
                    //   min(0, NaN) -> NaN
                    //   min(NaN, 0) -> 0
                    dest[i] = (src1[i] < src2[i]) ? src1[i] : src2[i];
                }
                Record<DebugDataRecord::DEST_OUT>(state.debug, iteration, dest);
                break;

            case OpCode::Id::DP3:
            case OpCode::Id::DP4:
            case OpCode::Id::DPH:
            case OpCode::Id::DPHI:
            {
                Record<DebugDataRecord::SRC1>(state.debug, iteration, src1);
                Record<DebugDataRecord::SRC2>(state.debug, iteration, src2);
                Record<DebugDataRecord::DEST_IN>(state.debug, iteration, dest);

                OpCode::Id opcode = instr.opcode.Value().EffectiveOpCode();
                if (opcode == OpCode::Id::DPH || opcode == OpCode::Id::DPHI)
                    src1[3] = float24::FromFloat32(1.0f);

                int num_components = (opcode == OpCode::Id::DP3) ? 3 : 4;
                float24 dot = std::inner_product(src1, src1 + num_components, src2, float24::FromFloat32(0.f));

                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = dot;
                }
                Record<DebugDataRecord::DEST_OUT>(state.debug, iteration, dest);
                break;
            }

            // Reciprocal
            case OpCode::Id::RCP:
            {
                Record<DebugDataRecord::SRC1>(state.debug, iteration, src1);
                Record<DebugDataRecord::DEST_IN>(state.debug, iteration, dest);
                float24 rcp_res = float24::FromFloat32(1.0f / src1[0].ToFloat32());
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = rcp_res;
                }
                Record<DebugDataRecord::DEST_OUT>(state.debug, iteration, dest);
                break;
            }

            // Reciprocal Square Root
            case OpCode::Id::RSQ:
            {
                Record<DebugDataRecord::SRC1>(state.debug, iteration, src1);
                Record<DebugDataRecord::DEST_IN>(state.debug, iteration, dest);
                float24 rsq_res = float24::FromFloat32(1.0f / std::sqrt(src1[0].ToFloat32()));
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = rsq_res;
                }
                Record<DebugDataRecord::DEST_OUT>(state.debug, iteration, dest);
                break;
            }

            case OpCode::Id::MOVA:
            {
                Record<DebugDataRecord::SRC1>(state.debug, iteration, src1);
                for (int i = 0; i < 2; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    // TODO: Figure out how the rounding is done on hardware
                    state.address_registers[i] = static_cast<s32>(src1[i].ToFloat32());
                }
                Record<DebugDataRecord::ADDR_REG_OUT>(state.debug, iteration, state.address_registers);
                break;
            }

            case OpCode::Id::MOV:
            {
                Record<DebugDataRecord::SRC1>(state.debug, iteration, src1);
                Record<DebugDataRecord::DEST_IN>(state.debug, iteration, dest);
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = src1[i];
                }
                Record<DebugDataRecord::DEST_OUT>(state.debug, iteration, dest);
                break;
            }

            case OpCode::Id::SGE:
            case OpCode::Id::SGEI:
                Record<DebugDataRecord::SRC1>(state.debug, iteration, src1);
                Record<DebugDataRecord::SRC2>(state.debug, iteration, src2);
                Record<DebugDataRecord::DEST_IN>(state.debug, iteration, dest);
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = (src1[i] >= src2[i]) ? float24::FromFloat32(1.0f) : float24::FromFloat32(0.0f);
                }
                Record<DebugDataRecord::DEST_OUT>(state.debug, iteration, dest);
                break;

            case OpCode::Id::SLT:
            case OpCode::Id::SLTI:
                Record<DebugDataRecord::SRC1>(state.debug, iteration, src1);
                Record<DebugDataRecord::SRC2>(state.debug, iteration, src2);
                Record<DebugDataRecord::DEST_IN>(state.debug, iteration, dest);
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = (src1[i] < src2[i]) ? float24::FromFloat32(1.0f) : float24::FromFloat32(0.0f);
                }
                Record<DebugDataRecord::DEST_OUT>(state.debug, iteration, dest);
                break;

            case OpCode::Id::CMP:
                Record<DebugDataRecord::SRC1>(state.debug, iteration, src1);
                Record<DebugDataRecord::SRC2>(state.debug, iteration, src2);
                for (int i = 0; i < 2; ++i) {
                    // TODO: Can you restrict to one compare via dest masking?

                    auto compare_op = instr.common.compare_op;
                    auto op = (i == 0) ? compare_op.x.Value() : compare_op.y.Value();

                    switch (op) {
                        case Instruction::Common::CompareOpType::Equal:
                            state.conditional_code[i] = (src1[i] == src2[i]);
                            break;

                        case Instruction::Common::CompareOpType::NotEqual:
                            state.conditional_code[i] = (src1[i] != src2[i]);
                            break;

                        case Instruction::Common::CompareOpType::LessThan:
                            state.conditional_code[i] = (src1[i] <  src2[i]);
                            break;

                        case Instruction::Common::CompareOpType::LessEqual:
                            state.conditional_code[i] = (src1[i] <= src2[i]);
                            break;

                        case Instruction::Common::CompareOpType::GreaterThan:
                            state.conditional_code[i] = (src1[i] >  src2[i]);
                            break;

                        case Instruction::Common::CompareOpType::GreaterEqual:
                            state.conditional_code[i] = (src1[i] >= src2[i]);
                            break;

                        default:
                            LOG_ERROR(HW_GPU, "Unknown compare mode %x", static_cast<int>(op));
                            break;
                    }
                }
                Record<DebugDataRecord::CMP_RESULT>(state.debug, iteration, state.conditional_code);
                break;

            case OpCode::Id::EX2:
            {
                Record<DebugDataRecord::SRC1>(state.debug, iteration, src1);
                Record<DebugDataRecord::DEST_IN>(state.debug, iteration, dest);

                // EX2 only takes first component exp2 and writes it to all dest components
                float24 ex2_res = float24::FromFloat32(std::exp2(src1[0].ToFloat32()));
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = ex2_res;
                }

                Record<DebugDataRecord::DEST_OUT>(state.debug, iteration, dest);
                break;
            }

            case OpCode::Id::LG2:
            {
                Record<DebugDataRecord::SRC1>(state.debug, iteration, src1);
                Record<DebugDataRecord::DEST_IN>(state.debug, iteration, dest);

                // LG2 only takes the first component log2 and writes it to all dest components
                float24 lg2_res = float24::FromFloat32(std::log2(src1[0].ToFloat32()));
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = lg2_res;
                }

                Record<DebugDataRecord::DEST_OUT>(state.debug, iteration, dest);
                break;
            }

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
                const SwizzlePattern& swizzle = *reinterpret_cast<const SwizzlePattern*>(&swizzle_data[instr.mad.operand_desc_id]);

                bool is_inverted = (instr.opcode.Value().EffectiveOpCode() == OpCode::Id::MADI);

                const int address_offset = (instr.mad.address_register_index == 0)
                                           ? 0 : state.address_registers[instr.mad.address_register_index - 1];

                const float24* src1_ = LookupSourceRegister(instr.mad.GetSrc1(is_inverted));
                const float24* src2_ = LookupSourceRegister(instr.mad.GetSrc2(is_inverted) + (!is_inverted * address_offset));
                const float24* src3_ = LookupSourceRegister(instr.mad.GetSrc3(is_inverted) + ( is_inverted * address_offset));

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

                float24* dest = (instr.mad.dest.Value() < 0x10) ? &state.output_registers.value[instr.mad.dest.Value().GetIndex()][0]
                            : (instr.mad.dest.Value() < 0x20) ? &state.registers.temporary[instr.mad.dest.Value().GetIndex()][0]
                            : dummy_vec4_float24;

                Record<DebugDataRecord::SRC1>(state.debug, iteration, src1);
                Record<DebugDataRecord::SRC2>(state.debug, iteration, src2);
                Record<DebugDataRecord::SRC3>(state.debug, iteration, src3);
                Record<DebugDataRecord::DEST_IN>(state.debug, iteration, dest);
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = src1[i] * src2[i] + src3[i];
                }
                Record<DebugDataRecord::DEST_OUT>(state.debug, iteration, dest);
            } else {
                LOG_ERROR(HW_GPU, "Unhandled multiply-add instruction: 0x%02x (%s): 0x%08x",
                          (int)instr.opcode.Value().EffectiveOpCode(), instr.opcode.Value().GetInfo().name, instr.hex);
            }
            break;
        }

        default:
        {
            static auto evaluate_condition = [](const UnitState<Debug>& state, bool refx, bool refy, Instruction::FlowControlType flow_control) {
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
                Record<DebugDataRecord::COND_CMP_IN>(state.debug, iteration, state.conditional_code);
                if (evaluate_condition(state, instr.flow_control.refx, instr.flow_control.refy, instr.flow_control)) {
                    program_counter = instr.flow_control.dest_offset - 1;
                }
                break;

            case OpCode::Id::JMPU:
                Record<DebugDataRecord::COND_BOOL_IN>(state.debug, iteration, uniforms.b[instr.flow_control.bool_uniform_id]);

                if (uniforms.b[instr.flow_control.bool_uniform_id] == !(instr.flow_control.num_instructions & 1)) {
                    program_counter = instr.flow_control.dest_offset - 1;
                }
                break;

            case OpCode::Id::CALL:
                call(state,
                     instr.flow_control.dest_offset,
                     instr.flow_control.num_instructions,
                     program_counter + 1, 0, 0);
                break;

            case OpCode::Id::CALLU:
                Record<DebugDataRecord::COND_BOOL_IN>(state.debug, iteration, uniforms.b[instr.flow_control.bool_uniform_id]);
                if (uniforms.b[instr.flow_control.bool_uniform_id]) {
                    call(state,
                        instr.flow_control.dest_offset,
                        instr.flow_control.num_instructions,
                        program_counter + 1, 0, 0);
                }
                break;

            case OpCode::Id::CALLC:
                Record<DebugDataRecord::COND_CMP_IN>(state.debug, iteration, state.conditional_code);
                if (evaluate_condition(state, instr.flow_control.refx, instr.flow_control.refy, instr.flow_control)) {
                    call(state,
                        instr.flow_control.dest_offset,
                        instr.flow_control.num_instructions,
                        program_counter + 1, 0, 0);
                }
                break;

            case OpCode::Id::NOP:
                break;

            case OpCode::Id::IFU:
                Record<DebugDataRecord::COND_BOOL_IN>(state.debug, iteration, uniforms.b[instr.flow_control.bool_uniform_id]);
                if (uniforms.b[instr.flow_control.bool_uniform_id]) {
                    call(state,
                         program_counter + 1,
                         instr.flow_control.dest_offset - program_counter - 1,
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

                Record<DebugDataRecord::COND_CMP_IN>(state.debug, iteration, state.conditional_code);
                if (evaluate_condition(state, instr.flow_control.refx, instr.flow_control.refy, instr.flow_control)) {
                    call(state,
                         program_counter + 1,
                         instr.flow_control.dest_offset - program_counter - 1,
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
                Math::Vec4<u8> loop_param(uniforms.i[instr.flow_control.int_uniform_id].x,
                                          uniforms.i[instr.flow_control.int_uniform_id].y,
                                          uniforms.i[instr.flow_control.int_uniform_id].z,
                                          uniforms.i[instr.flow_control.int_uniform_id].w);
                state.address_registers[2] = loop_param.y;

                Record<DebugDataRecord::LOOP_INT_IN>(state.debug, iteration, loop_param);
                call(state,
                     program_counter + 1,
                     instr.flow_control.dest_offset - program_counter + 1,
                     instr.flow_control.dest_offset + 1,
                     loop_param.x,
                     loop_param.z);
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

        ++program_counter;
        ++iteration;
    }
}

// Explicit instantiation
template void RunInterpreter(const ShaderSetup& setup, UnitState<false>& state, unsigned offset);
template void RunInterpreter(const ShaderSetup& setup, UnitState<true>& state, unsigned offset);

} // namespace

} // namespace
