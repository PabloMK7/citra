// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/container/static_vector.hpp>
#include <boost/range/algorithm.hpp>

#include <common/file_util.h>

#include <nihstro/shader_bytecode.h>

#include "common/profiler.h"

#include "pica.h"
#include "vertex_shader.h"
#include "debug_utils/debug_utils.h"

using nihstro::OpCode;
using nihstro::Instruction;
using nihstro::RegisterType;
using nihstro::SourceRegister;
using nihstro::SwizzlePattern;

namespace Pica {

namespace VertexShader {

struct VertexShaderState {
    u32 program_counter;

    const float24* input_register_table[16];
    Math::Vec4<float24> output_registers[16];

    Math::Vec4<float24> temporary_registers[16];
    bool conditional_code[2];

    // Two Address registers and one loop counter
    // TODO: How many bits do these actually have?
    s32 address_registers[3];

    enum {
        INVALID_ADDRESS = 0xFFFFFFFF
    };

    struct CallStackElement {
        u32 final_address;  // Address upon which we jump to return_address
        u32 return_address; // Where to jump when leaving scope
        u8 repeat_counter;  // How often to repeat until this call stack element is removed
        u8 loop_increment;  // Which value to add to the loop counter after an iteration
                            // TODO: Should this be a signed value? Does it even matter?
        u32 loop_address;   // The address where we'll return to after each loop iteration
    };

    // TODO: Is there a maximal size for this?
    boost::container::static_vector<CallStackElement, 16> call_stack;

    struct {
        u32 max_offset; // maximum program counter ever reached
        u32 max_opdesc_id; // maximum swizzle pattern index ever used
    } debug;
};

static void ProcessShaderCode(VertexShaderState& state) {
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

        static auto call = [](VertexShaderState& state, u32 offset, u32 num_instructions,
                              u32 return_offset, u8 repeat_count, u8 loop_increment) {
            state.program_counter = offset - 1; // -1 to make sure when incrementing the PC we end up at the correct offset
            ASSERT(state.call_stack.size() < state.call_stack.capacity());
            state.call_stack.push_back({ offset + num_instructions, return_offset, repeat_count, loop_increment, offset });
        };
        state.debug.max_offset = std::max<u32>(state.debug.max_offset, 1 + state.program_counter);

        auto LookupSourceRegister = [&](const SourceRegister& source_reg) -> const float24* {
            switch (source_reg.GetRegisterType()) {
            case RegisterType::Input:
                return state.input_register_table[source_reg.GetIndex()];

            case RegisterType::Temporary:
                return &state.temporary_registers[source_reg.GetIndex()].x;

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

            float24* dest = (instr.common.dest.Value() < 0x10) ? &state.output_registers[instr.common.dest.Value().GetIndex()][0]
                        : (instr.common.dest.Value() < 0x20) ? &state.temporary_registers[instr.common.dest.Value().GetIndex()][0]
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

                float24* dest = (instr.mad.dest.Value() < 0x10) ? &state.output_registers[instr.mad.dest.Value().GetIndex()][0]
                            : (instr.mad.dest.Value() < 0x20) ? &state.temporary_registers[instr.mad.dest.Value().GetIndex()][0]
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
            static auto evaluate_condition = [](const VertexShaderState& state, bool refx, bool refy, Instruction::FlowControlType flow_control) {
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

static Common::Profiling::TimingCategory shader_category("Vertex Shader");

OutputVertex RunShader(const InputVertex& input, int num_attributes, const Regs::ShaderConfig& config, const State::ShaderSetup& setup) {
    Common::Profiling::ScopeTimer timer(shader_category);

    VertexShaderState state;

    state.program_counter = config.main_offset;
    state.debug.max_offset = 0;
    state.debug.max_opdesc_id = 0;

    // Setup input register table
    const auto& attribute_register_map = config.input_register_map;
    float24 dummy_register;
    boost::fill(state.input_register_table, &dummy_register);

    if (num_attributes > 0) state.input_register_table[attribute_register_map.attribute0_register] = &input.attr[0].x;
    if (num_attributes > 1) state.input_register_table[attribute_register_map.attribute1_register] = &input.attr[1].x;
    if (num_attributes > 2) state.input_register_table[attribute_register_map.attribute2_register] = &input.attr[2].x;
    if (num_attributes > 3) state.input_register_table[attribute_register_map.attribute3_register] = &input.attr[3].x;
    if (num_attributes > 4) state.input_register_table[attribute_register_map.attribute4_register] = &input.attr[4].x;
    if (num_attributes > 5) state.input_register_table[attribute_register_map.attribute5_register] = &input.attr[5].x;
    if (num_attributes > 6) state.input_register_table[attribute_register_map.attribute6_register] = &input.attr[6].x;
    if (num_attributes > 7) state.input_register_table[attribute_register_map.attribute7_register] = &input.attr[7].x;
    if (num_attributes > 8) state.input_register_table[attribute_register_map.attribute8_register] = &input.attr[8].x;
    if (num_attributes > 9) state.input_register_table[attribute_register_map.attribute9_register] = &input.attr[9].x;
    if (num_attributes > 10) state.input_register_table[attribute_register_map.attribute10_register] = &input.attr[10].x;
    if (num_attributes > 11) state.input_register_table[attribute_register_map.attribute11_register] = &input.attr[11].x;
    if (num_attributes > 12) state.input_register_table[attribute_register_map.attribute12_register] = &input.attr[12].x;
    if (num_attributes > 13) state.input_register_table[attribute_register_map.attribute13_register] = &input.attr[13].x;
    if (num_attributes > 14) state.input_register_table[attribute_register_map.attribute14_register] = &input.attr[14].x;
    if (num_attributes > 15) state.input_register_table[attribute_register_map.attribute15_register] = &input.attr[15].x;

    state.conditional_code[0] = false;
    state.conditional_code[1] = false;

    ProcessShaderCode(state);
#if PICA_DUMP_SHADERS
    DebugUtils::DumpShader(setup.program_code.data(), state.debug.max_offset, setup.swizzle_data.data(),
                           state.debug.max_opdesc_id, config.main_offset,
                           g_state.regs.vs_output_attributes); // TODO: Don't hardcode VS here
#endif

    // Setup output data
    OutputVertex ret;
    // TODO(neobrain): Under some circumstances, up to 16 attributes may be output. We need to
    // figure out what those circumstances are and enable the remaining outputs then.
    for (int i = 0; i < 7; ++i) {
        const auto& output_register_map = g_state.regs.vs_output_attributes[i]; // TODO: Don't hardcode VS here

        u32 semantics[4] = {
            output_register_map.map_x, output_register_map.map_y,
            output_register_map.map_z, output_register_map.map_w
        };

        for (int comp = 0; comp < 4; ++comp) {
            float24* out = ((float24*)&ret) + semantics[comp];
            if (semantics[comp] != Regs::VSOutputAttributes::INVALID) {
                *out = state.output_registers[i][comp];
            } else {
                // Zero output so that attributes which aren't output won't have denormals in them,
                // which would slow us down later.
                memset(out, 0, sizeof(*out));
            }
        }
    }

    // The hardware takes the absolute and saturates vertex colors like this, *before* doing interpolation
    for (int i = 0; i < 4; ++i) {
        ret.color[i] = float24::FromFloat32(
            std::fmin(std::fabs(ret.color[i].ToFloat32()), 1.0f));
    }

    LOG_TRACE(Render_Software, "Output vertex: pos (%.2f, %.2f, %.2f, %.2f), col(%.2f, %.2f, %.2f, %.2f), tc0(%.2f, %.2f)",
        ret.pos.x.ToFloat32(), ret.pos.y.ToFloat32(), ret.pos.z.ToFloat32(), ret.pos.w.ToFloat32(),
        ret.color.x.ToFloat32(), ret.color.y.ToFloat32(), ret.color.z.ToFloat32(), ret.color.w.ToFloat32(),
        ret.tc0.u().ToFloat32(), ret.tc0.v().ToFloat32());

    return ret;
}


} // namespace

} // namespace
