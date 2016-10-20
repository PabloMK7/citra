// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cstddef>
#include <memory>
#include <utility>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/vector_math.h"
#include "core/hle/service/gsp_gpu.h"
#include "core/hw/gpu.h"
#include "core/memory.h"
#include "core/tracer/recorder.h"
#include "video_core/command_processor.h"
#include "video_core/debug_utils/debug_utils.h"
#include "video_core/pica.h"
#include "video_core/pica_state.h"
#include "video_core/pica_types.h"
#include "video_core/primitive_assembly.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_base.h"
#include "video_core/shader/shader.h"
#include "video_core/vertex_loader.h"
#include "video_core/video_core.h"

namespace Pica {

namespace CommandProcessor {

static int float_regs_counter = 0;

static u32 uniform_write_buffer[4];

static int default_attr_counter = 0;

static u32 default_attr_write_buffer[3];

// Expand a 4-bit mask to 4-byte mask, e.g. 0b0101 -> 0x00FF00FF
static const u32 expand_bits_to_bytes[] = {
    0x00000000, 0x000000ff, 0x0000ff00, 0x0000ffff, 0x00ff0000, 0x00ff00ff, 0x00ffff00, 0x00ffffff,
    0xff000000, 0xff0000ff, 0xff00ff00, 0xff00ffff, 0xffff0000, 0xffff00ff, 0xffffff00, 0xffffffff,
};

MICROPROFILE_DEFINE(GPU_Drawing, "GPU", "Drawing", MP_RGB(50, 50, 240));

static void WritePicaReg(u32 id, u32 value, u32 mask) {
    auto& regs = g_state.regs;

    if (id >= regs.NumIds())
        return;

    // If we're skipping this frame, only allow trigger IRQ
    if (GPU::g_skip_frame && id != PICA_REG_INDEX(trigger_irq))
        return;

    // TODO: Figure out how register masking acts on e.g. vs.uniform_setup.set_value
    u32 old_value = regs[id];

    const u32 write_mask = expand_bits_to_bytes[mask];

    regs[id] = (old_value & ~write_mask) | (value & write_mask);

    DebugUtils::OnPicaRegWrite({(u16)id, (u16)mask, regs[id]});

    if (g_debug_context)
        g_debug_context->OnEvent(DebugContext::Event::PicaCommandLoaded,
                                 reinterpret_cast<void*>(&id));

    switch (id) {
    // Trigger IRQ
    case PICA_REG_INDEX(trigger_irq):
        GSP_GPU::SignalInterrupt(GSP_GPU::InterruptId::P3D);
        break;

    case PICA_REG_INDEX_WORKAROUND(triangle_topology, 0x25E):
        g_state.primitive_assembler.Reconfigure(regs.triangle_topology);
        break;

    case PICA_REG_INDEX_WORKAROUND(restart_primitive, 0x25F):
        g_state.primitive_assembler.Reset();
        break;

    case PICA_REG_INDEX_WORKAROUND(vs_default_attributes_setup.index, 0x232):
        g_state.immediate.current_attribute = 0;
        default_attr_counter = 0;
        break;

    // Load default vertex input attributes
    case PICA_REG_INDEX_WORKAROUND(vs_default_attributes_setup.set_value[0], 0x233):
    case PICA_REG_INDEX_WORKAROUND(vs_default_attributes_setup.set_value[1], 0x234):
    case PICA_REG_INDEX_WORKAROUND(vs_default_attributes_setup.set_value[2], 0x235): {
        // TODO: Does actual hardware indeed keep an intermediate buffer or does
        //       it directly write the values?
        default_attr_write_buffer[default_attr_counter++] = value;

        // Default attributes are written in a packed format such that four float24 values are
        // encoded in
        // three 32-bit numbers. We write to internal memory once a full such vector is
        // written.
        if (default_attr_counter >= 3) {
            default_attr_counter = 0;

            auto& setup = regs.vs_default_attributes_setup;

            if (setup.index >= 16) {
                LOG_ERROR(HW_GPU, "Invalid VS default attribute index %d", (int)setup.index);
                break;
            }

            Math::Vec4<float24> attribute;

            // NOTE: The destination component order indeed is "backwards"
            attribute.w = float24::FromRaw(default_attr_write_buffer[0] >> 8);
            attribute.z = float24::FromRaw(((default_attr_write_buffer[0] & 0xFF) << 16) |
                                           ((default_attr_write_buffer[1] >> 16) & 0xFFFF));
            attribute.y = float24::FromRaw(((default_attr_write_buffer[1] & 0xFFFF) << 8) |
                                           ((default_attr_write_buffer[2] >> 24) & 0xFF));
            attribute.x = float24::FromRaw(default_attr_write_buffer[2] & 0xFFFFFF);

            LOG_TRACE(HW_GPU, "Set default VS attribute %x to (%f %f %f %f)", (int)setup.index,
                      attribute.x.ToFloat32(), attribute.y.ToFloat32(), attribute.z.ToFloat32(),
                      attribute.w.ToFloat32());

            // TODO: Verify that this actually modifies the register!
            if (setup.index < 15) {
                g_state.vs_default_attributes[setup.index] = attribute;
                setup.index++;
            } else {
                // Put each attribute into an immediate input buffer.
                // When all specified immediate attributes are present, the Vertex Shader is invoked
                // and everything is
                // sent to the primitive assembler.

                auto& immediate_input = g_state.immediate.input_vertex;
                auto& immediate_attribute_id = g_state.immediate.current_attribute;

                immediate_input.attr[immediate_attribute_id++] = attribute;

                if (immediate_attribute_id >= regs.vs.num_input_attributes + 1) {
                    immediate_attribute_id = 0;

                    Shader::UnitState<false> shader_unit;
                    g_state.vs.Setup();

                    // Send to vertex shader
                    if (g_debug_context)
                        g_debug_context->OnEvent(DebugContext::Event::VertexShaderInvocation,
                                                 static_cast<void*>(&immediate_input));
                    g_state.vs.Run(shader_unit, immediate_input, regs.vs.num_input_attributes + 1);
                    Shader::OutputVertex output_vertex =
                        shader_unit.output_registers.ToVertex(regs.vs);

                    // Send to renderer
                    using Pica::Shader::OutputVertex;
                    auto AddTriangle = [](const OutputVertex& v0, const OutputVertex& v1,
                                          const OutputVertex& v2) {
                        VideoCore::g_renderer->Rasterizer()->AddTriangle(v0, v1, v2);
                    };

                    g_state.primitive_assembler.SubmitVertex(output_vertex, AddTriangle);
                }
            }
        }
        break;
    }

    case PICA_REG_INDEX(gpu_mode):
        if (regs.gpu_mode == Regs::GPUMode::Configuring) {
            // Draw immediate mode triangles when GPU Mode is set to GPUMode::Configuring
            VideoCore::g_renderer->Rasterizer()->DrawTriangles();

            if (g_debug_context) {
                g_debug_context->OnEvent(DebugContext::Event::FinishedPrimitiveBatch, nullptr);
            }
        }
        break;

    case PICA_REG_INDEX_WORKAROUND(command_buffer.trigger[0], 0x23c):
    case PICA_REG_INDEX_WORKAROUND(command_buffer.trigger[1], 0x23d): {
        unsigned index = static_cast<unsigned>(id - PICA_REG_INDEX(command_buffer.trigger[0]));
        u32* head_ptr =
            (u32*)Memory::GetPhysicalPointer(regs.command_buffer.GetPhysicalAddress(index));
        g_state.cmd_list.head_ptr = g_state.cmd_list.current_ptr = head_ptr;
        g_state.cmd_list.length = regs.command_buffer.GetSize(index) / sizeof(u32);
        break;
    }

    // It seems like these trigger vertex rendering
    case PICA_REG_INDEX(trigger_draw):
    case PICA_REG_INDEX(trigger_draw_indexed): {
        MICROPROFILE_SCOPE(GPU_Drawing);

#if PICA_LOG_TEV
        DebugUtils::DumpTevStageConfig(regs.GetTevStages());
#endif
        if (g_debug_context)
            g_debug_context->OnEvent(DebugContext::Event::IncomingPrimitiveBatch, nullptr);

        // Processes information about internal vertex attributes to figure out how a vertex is
        // loaded.
        // Later, these can be compiled and cached.
        const u32 base_address = regs.vertex_attributes.GetPhysicalBaseAddress();
        VertexLoader loader(regs);

        // Load vertices
        bool is_indexed = (id == PICA_REG_INDEX(trigger_draw_indexed));

        const auto& index_info = regs.index_array;
        const u8* index_address_8 = Memory::GetPhysicalPointer(base_address + index_info.offset);
        const u16* index_address_16 = reinterpret_cast<const u16*>(index_address_8);
        bool index_u16 = index_info.format != 0;

        PrimitiveAssembler<Shader::OutputVertex>& primitive_assembler = g_state.primitive_assembler;

        if (g_debug_context) {
            for (int i = 0; i < 3; ++i) {
                const auto texture = regs.GetTextures()[i];
                if (!texture.enabled)
                    continue;

                u8* texture_data = Memory::GetPhysicalPointer(texture.config.GetPhysicalAddress());
                if (g_debug_context && Pica::g_debug_context->recorder)
                    g_debug_context->recorder->MemoryAccessed(
                        texture_data, Pica::Regs::NibblesPerPixel(texture.format) *
                                          texture.config.width / 2 * texture.config.height,
                        texture.config.GetPhysicalAddress());
            }
        }

        DebugUtils::MemoryAccessTracker memory_accesses;

        // Simple circular-replacement vertex cache
        // The size has been tuned for optimal balance between hit-rate and the cost of lookup
        const size_t VERTEX_CACHE_SIZE = 32;
        std::array<u16, VERTEX_CACHE_SIZE> vertex_cache_ids;
        std::array<Shader::OutputRegisters, VERTEX_CACHE_SIZE> vertex_cache;

        unsigned int vertex_cache_pos = 0;
        vertex_cache_ids.fill(-1);

        Shader::UnitState<false> shader_unit;
        g_state.vs.Setup();

        for (unsigned int index = 0; index < regs.num_vertices; ++index) {
            // Indexed rendering doesn't use the start offset
            unsigned int vertex =
                is_indexed ? (index_u16 ? index_address_16[index] : index_address_8[index])
                           : (index + regs.vertex_offset);

            // -1 is a common special value used for primitive restart. Since it's unknown if
            // the PICA supports it, and it would mess up the caching, guard against it here.
            ASSERT(vertex != -1);

            bool vertex_cache_hit = false;
            Shader::OutputRegisters output_registers;

            if (is_indexed) {
                if (g_debug_context && Pica::g_debug_context->recorder) {
                    int size = index_u16 ? 2 : 1;
                    memory_accesses.AddAccess(base_address + index_info.offset + size * index,
                                              size);
                }

                for (unsigned int i = 0; i < VERTEX_CACHE_SIZE; ++i) {
                    if (vertex == vertex_cache_ids[i]) {
                        output_registers = vertex_cache[i];
                        vertex_cache_hit = true;
                        break;
                    }
                }
            }

            if (!vertex_cache_hit) {
                // Initialize data for the current vertex
                Shader::InputVertex input;
                loader.LoadVertex(base_address, index, vertex, input, memory_accesses);

                // Send to vertex shader
                if (g_debug_context)
                    g_debug_context->OnEvent(DebugContext::Event::VertexShaderInvocation,
                                             (void*)&input);
                g_state.vs.Run(shader_unit, input, loader.GetNumTotalAttributes());
                output_registers = shader_unit.output_registers;

                if (is_indexed) {
                    vertex_cache[vertex_cache_pos] = output_registers;
                    vertex_cache_ids[vertex_cache_pos] = vertex;
                    vertex_cache_pos = (vertex_cache_pos + 1) % VERTEX_CACHE_SIZE;
                }
            }

            // Retrieve vertex from register data
            Shader::OutputVertex output_vertex = output_registers.ToVertex(regs.vs);

            // Send to renderer
            using Pica::Shader::OutputVertex;
            auto AddTriangle = [](const OutputVertex& v0, const OutputVertex& v1,
                                  const OutputVertex& v2) {
                VideoCore::g_renderer->Rasterizer()->AddTriangle(v0, v1, v2);
            };

            primitive_assembler.SubmitVertex(output_vertex, AddTriangle);
        }

        for (auto& range : memory_accesses.ranges) {
            g_debug_context->recorder->MemoryAccessed(Memory::GetPhysicalPointer(range.first),
                                                      range.second, range.first);
        }

        break;
    }

    case PICA_REG_INDEX(vs.bool_uniforms):
        for (unsigned i = 0; i < 16; ++i)
            g_state.vs.uniforms.b[i] = (regs.vs.bool_uniforms.Value() & (1 << i)) != 0;

        break;

    case PICA_REG_INDEX_WORKAROUND(vs.int_uniforms[0], 0x2b1):
    case PICA_REG_INDEX_WORKAROUND(vs.int_uniforms[1], 0x2b2):
    case PICA_REG_INDEX_WORKAROUND(vs.int_uniforms[2], 0x2b3):
    case PICA_REG_INDEX_WORKAROUND(vs.int_uniforms[3], 0x2b4): {
        int index = (id - PICA_REG_INDEX_WORKAROUND(vs.int_uniforms[0], 0x2b1));
        auto values = regs.vs.int_uniforms[index];
        g_state.vs.uniforms.i[index] = Math::Vec4<u8>(values.x, values.y, values.z, values.w);
        LOG_TRACE(HW_GPU, "Set integer uniform %d to %02x %02x %02x %02x", index, values.x.Value(),
                  values.y.Value(), values.z.Value(), values.w.Value());
        break;
    }

    case PICA_REG_INDEX_WORKAROUND(vs.uniform_setup.set_value[0], 0x2c1):
    case PICA_REG_INDEX_WORKAROUND(vs.uniform_setup.set_value[1], 0x2c2):
    case PICA_REG_INDEX_WORKAROUND(vs.uniform_setup.set_value[2], 0x2c3):
    case PICA_REG_INDEX_WORKAROUND(vs.uniform_setup.set_value[3], 0x2c4):
    case PICA_REG_INDEX_WORKAROUND(vs.uniform_setup.set_value[4], 0x2c5):
    case PICA_REG_INDEX_WORKAROUND(vs.uniform_setup.set_value[5], 0x2c6):
    case PICA_REG_INDEX_WORKAROUND(vs.uniform_setup.set_value[6], 0x2c7):
    case PICA_REG_INDEX_WORKAROUND(vs.uniform_setup.set_value[7], 0x2c8): {
        auto& uniform_setup = regs.vs.uniform_setup;

        // TODO: Does actual hardware indeed keep an intermediate buffer or does
        //       it directly write the values?
        uniform_write_buffer[float_regs_counter++] = value;

        // Uniforms are written in a packed format such that four float24 values are encoded in
        // three 32-bit numbers. We write to internal memory once a full such vector is
        // written.
        if ((float_regs_counter >= 4 && uniform_setup.IsFloat32()) ||
            (float_regs_counter >= 3 && !uniform_setup.IsFloat32())) {
            float_regs_counter = 0;

            auto& uniform = g_state.vs.uniforms.f[uniform_setup.index];

            if (uniform_setup.index > 95) {
                LOG_ERROR(HW_GPU, "Invalid VS uniform index %d", (int)uniform_setup.index);
                break;
            }

            // NOTE: The destination component order indeed is "backwards"
            if (uniform_setup.IsFloat32()) {
                for (auto i : {0, 1, 2, 3})
                    uniform[3 - i] = float24::FromFloat32(*(float*)(&uniform_write_buffer[i]));
            } else {
                // TODO: Untested
                uniform.w = float24::FromRaw(uniform_write_buffer[0] >> 8);
                uniform.z = float24::FromRaw(((uniform_write_buffer[0] & 0xFF) << 16) |
                                             ((uniform_write_buffer[1] >> 16) & 0xFFFF));
                uniform.y = float24::FromRaw(((uniform_write_buffer[1] & 0xFFFF) << 8) |
                                             ((uniform_write_buffer[2] >> 24) & 0xFF));
                uniform.x = float24::FromRaw(uniform_write_buffer[2] & 0xFFFFFF);
            }

            LOG_TRACE(HW_GPU, "Set uniform %x to (%f %f %f %f)", (int)uniform_setup.index,
                      uniform.x.ToFloat32(), uniform.y.ToFloat32(), uniform.z.ToFloat32(),
                      uniform.w.ToFloat32());

            // TODO: Verify that this actually modifies the register!
            uniform_setup.index.Assign(uniform_setup.index + 1);
        }
        break;
    }

    // Load shader program code
    case PICA_REG_INDEX_WORKAROUND(vs.program.set_word[0], 0x2cc):
    case PICA_REG_INDEX_WORKAROUND(vs.program.set_word[1], 0x2cd):
    case PICA_REG_INDEX_WORKAROUND(vs.program.set_word[2], 0x2ce):
    case PICA_REG_INDEX_WORKAROUND(vs.program.set_word[3], 0x2cf):
    case PICA_REG_INDEX_WORKAROUND(vs.program.set_word[4], 0x2d0):
    case PICA_REG_INDEX_WORKAROUND(vs.program.set_word[5], 0x2d1):
    case PICA_REG_INDEX_WORKAROUND(vs.program.set_word[6], 0x2d2):
    case PICA_REG_INDEX_WORKAROUND(vs.program.set_word[7], 0x2d3): {
        g_state.vs.program_code[regs.vs.program.offset] = value;
        regs.vs.program.offset++;
        break;
    }

    // Load swizzle pattern data
    case PICA_REG_INDEX_WORKAROUND(vs.swizzle_patterns.set_word[0], 0x2d6):
    case PICA_REG_INDEX_WORKAROUND(vs.swizzle_patterns.set_word[1], 0x2d7):
    case PICA_REG_INDEX_WORKAROUND(vs.swizzle_patterns.set_word[2], 0x2d8):
    case PICA_REG_INDEX_WORKAROUND(vs.swizzle_patterns.set_word[3], 0x2d9):
    case PICA_REG_INDEX_WORKAROUND(vs.swizzle_patterns.set_word[4], 0x2da):
    case PICA_REG_INDEX_WORKAROUND(vs.swizzle_patterns.set_word[5], 0x2db):
    case PICA_REG_INDEX_WORKAROUND(vs.swizzle_patterns.set_word[6], 0x2dc):
    case PICA_REG_INDEX_WORKAROUND(vs.swizzle_patterns.set_word[7], 0x2dd): {
        g_state.vs.swizzle_data[regs.vs.swizzle_patterns.offset] = value;
        regs.vs.swizzle_patterns.offset++;
        break;
    }

    case PICA_REG_INDEX_WORKAROUND(lighting.lut_data[0], 0x1c8):
    case PICA_REG_INDEX_WORKAROUND(lighting.lut_data[1], 0x1c9):
    case PICA_REG_INDEX_WORKAROUND(lighting.lut_data[2], 0x1ca):
    case PICA_REG_INDEX_WORKAROUND(lighting.lut_data[3], 0x1cb):
    case PICA_REG_INDEX_WORKAROUND(lighting.lut_data[4], 0x1cc):
    case PICA_REG_INDEX_WORKAROUND(lighting.lut_data[5], 0x1cd):
    case PICA_REG_INDEX_WORKAROUND(lighting.lut_data[6], 0x1ce):
    case PICA_REG_INDEX_WORKAROUND(lighting.lut_data[7], 0x1cf): {
        auto& lut_config = regs.lighting.lut_config;

        ASSERT_MSG(lut_config.index < 256, "lut_config.index exceeded maximum value of 255!");

        g_state.lighting.luts[lut_config.type][lut_config.index].raw = value;
        lut_config.index.Assign(lut_config.index + 1);
        break;
    }

    case PICA_REG_INDEX_WORKAROUND(fog_lut_data[0], 0xe8):
    case PICA_REG_INDEX_WORKAROUND(fog_lut_data[1], 0xe9):
    case PICA_REG_INDEX_WORKAROUND(fog_lut_data[2], 0xea):
    case PICA_REG_INDEX_WORKAROUND(fog_lut_data[3], 0xeb):
    case PICA_REG_INDEX_WORKAROUND(fog_lut_data[4], 0xec):
    case PICA_REG_INDEX_WORKAROUND(fog_lut_data[5], 0xed):
    case PICA_REG_INDEX_WORKAROUND(fog_lut_data[6], 0xee):
    case PICA_REG_INDEX_WORKAROUND(fog_lut_data[7], 0xef): {
        g_state.fog.lut[regs.fog_lut_offset % 128].raw = value;
        regs.fog_lut_offset.Assign(regs.fog_lut_offset + 1);
        break;
    }

    default:
        break;
    }

    VideoCore::g_renderer->Rasterizer()->NotifyPicaRegisterChanged(id);

    if (g_debug_context)
        g_debug_context->OnEvent(DebugContext::Event::PicaCommandProcessed,
                                 reinterpret_cast<void*>(&id));
}

void ProcessCommandList(const u32* list, u32 size) {
    g_state.cmd_list.head_ptr = g_state.cmd_list.current_ptr = list;
    g_state.cmd_list.length = size / sizeof(u32);

    while (g_state.cmd_list.current_ptr < g_state.cmd_list.head_ptr + g_state.cmd_list.length) {

        // Align read pointer to 8 bytes
        if ((g_state.cmd_list.head_ptr - g_state.cmd_list.current_ptr) % 2 != 0)
            ++g_state.cmd_list.current_ptr;

        u32 value = *g_state.cmd_list.current_ptr++;
        const CommandHeader header = {*g_state.cmd_list.current_ptr++};

        WritePicaReg(header.cmd_id, value, header.parameter_mask);

        for (unsigned i = 0; i < header.extra_data_length; ++i) {
            u32 cmd = header.cmd_id + (header.group_commands ? i + 1 : 0);
            WritePicaReg(cmd, *g_state.cmd_list.current_ptr++, header.parameter_mask);
        }
    }
}

} // namespace

} // namespace
