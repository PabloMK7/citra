// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include <boost/range/algorithm/fill.hpp>

#include "common/alignment.h"
#include "common/microprofile.h"
#include "common/profiler.h"

#include "core/settings.h"
#include "core/hle/service/gsp_gpu.h"
#include "core/hw/gpu.h"

#include "video_core/clipper.h"
#include "video_core/command_processor.h"
#include "video_core/pica.h"
#include "video_core/pica_state.h"
#include "video_core/primitive_assembly.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"
#include "video_core/debug_utils/debug_utils.h"
#include "video_core/shader/shader_interpreter.h"

namespace Pica {

namespace CommandProcessor {

static int float_regs_counter = 0;

static u32 uniform_write_buffer[4];

static int default_attr_counter = 0;

static u32 default_attr_write_buffer[3];

Common::Profiling::TimingCategory category_drawing("Drawing");

// Expand a 4-bit mask to 4-byte mask, e.g. 0b0101 -> 0x00FF00FF
static const u32 expand_bits_to_bytes[] = {
    0x00000000, 0x000000ff, 0x0000ff00, 0x0000ffff,
    0x00ff0000, 0x00ff00ff, 0x00ffff00, 0x00ffffff,
    0xff000000, 0xff0000ff, 0xff00ff00, 0xff00ffff,
    0xffff0000, 0xffff00ff, 0xffffff00, 0xffffffff
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

    DebugUtils::OnPicaRegWrite({ (u16)id, (u16)mask, regs[id] });

    if (g_debug_context)
        g_debug_context->OnEvent(DebugContext::Event::PicaCommandLoaded, reinterpret_cast<void*>(&id));

    switch(id) {
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
        case PICA_REG_INDEX_WORKAROUND(vs_default_attributes_setup.set_value[2], 0x235):
        {
            // TODO: Does actual hardware indeed keep an intermediate buffer or does
            //       it directly write the values?
            default_attr_write_buffer[default_attr_counter++] = value;

            // Default attributes are written in a packed format such that four float24 values are encoded in
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
                attribute.z = float24::FromRaw(((default_attr_write_buffer[0] & 0xFF) << 16) | ((default_attr_write_buffer[1] >> 16) & 0xFFFF));
                attribute.y = float24::FromRaw(((default_attr_write_buffer[1] & 0xFFFF) << 8) | ((default_attr_write_buffer[2] >> 24) & 0xFF));
                attribute.x = float24::FromRaw(default_attr_write_buffer[2] & 0xFFFFFF);

                LOG_TRACE(HW_GPU, "Set default VS attribute %x to (%f %f %f %f)", (int)setup.index,
                          attribute.x.ToFloat32(), attribute.y.ToFloat32(), attribute.z.ToFloat32(),
                          attribute.w.ToFloat32());

                // TODO: Verify that this actually modifies the register!
                if (setup.index < 15) {
                    g_state.vs.default_attributes[setup.index] = attribute;
                    setup.index++;
                } else {
                    // Put each attribute into an immediate input buffer.
                    // When all specified immediate attributes are present, the Vertex Shader is invoked and everything is
                    // sent to the primitive assembler.

                    auto& immediate_input = g_state.immediate.input_vertex;
                    auto& immediate_attribute_id = g_state.immediate.current_attribute;

                    immediate_input.attr[immediate_attribute_id++] = attribute;

                    if (immediate_attribute_id >= regs.vs.num_input_attributes+1) {
                        immediate_attribute_id = 0;

                        Shader::UnitState<false> shader_unit;
                        Shader::Setup();

                        if (g_debug_context)
                            g_debug_context->OnEvent(DebugContext::Event::VertexLoaded, static_cast<void*>(&immediate_input));

                        // Send to vertex shader
                        Shader::OutputVertex output = Shader::Run(shader_unit, immediate_input, regs.vs.num_input_attributes+1);

                        // Send to renderer
                        using Pica::Shader::OutputVertex;
                        auto AddTriangle = [](const OutputVertex& v0, const OutputVertex& v1, const OutputVertex& v2) {
                            VideoCore::g_renderer->Rasterizer()->AddTriangle(v0, v1, v2);
                        };

                        g_state.primitive_assembler.SubmitVertex(output, AddTriangle);
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
        case PICA_REG_INDEX_WORKAROUND(command_buffer.trigger[1], 0x23d):
        {
            unsigned index = static_cast<unsigned>(id - PICA_REG_INDEX(command_buffer.trigger[0]));
            u32* head_ptr = (u32*)Memory::GetPhysicalPointer(regs.command_buffer.GetPhysicalAddress(index));
            g_state.cmd_list.head_ptr = g_state.cmd_list.current_ptr = head_ptr;
            g_state.cmd_list.length = regs.command_buffer.GetSize(index) / sizeof(u32);
            break;
        }

        // It seems like these trigger vertex rendering
        case PICA_REG_INDEX(trigger_draw):
        case PICA_REG_INDEX(trigger_draw_indexed):
        {
            Common::Profiling::ScopeTimer scope_timer(category_drawing);
            MICROPROFILE_SCOPE(GPU_Drawing);

#if PICA_LOG_TEV
            DebugUtils::DumpTevStageConfig(regs.GetTevStages());
#endif

            if (g_debug_context)
                g_debug_context->OnEvent(DebugContext::Event::IncomingPrimitiveBatch, nullptr);

            const auto& attribute_config = regs.vertex_attributes;
            const u32 base_address = attribute_config.GetPhysicalBaseAddress();
            int num_total_attributes = attribute_config.GetNumTotalAttributes();

            // Information about internal vertex attributes
            u32 vertex_attribute_sources[16];
            boost::fill(vertex_attribute_sources, 0xdeadbeef);
            u32 vertex_attribute_strides[16] = {};
            Regs::VertexAttributeFormat vertex_attribute_formats[16] = {};

            u32 vertex_attribute_elements[16] = {};
            u32 vertex_attribute_element_size[16] = {};
            bool vertex_attribute_default[16] = {};
            // Setup attribute data from loaders
            for (int loader = 0; loader < 12; ++loader) {
                const auto& loader_config = attribute_config.attribute_loaders[loader];

                u32 offset = 0;

                // TODO: What happens if a loader overwrites a previous one's data?
                for (unsigned component = 0; component < loader_config.component_count; ++component) {
                    if (component >= 12) {
                        LOG_ERROR(HW_GPU, "Overflow in the vertex attribute loader %u trying to load component %u", loader, component);
                        continue;
                    }

                    u32 attribute_index = loader_config.GetComponent(component);
                    if (attribute_index < 12) {
                        int element_size = attribute_config.GetElementSizeInBytes(attribute_index);
                        offset = Common::AlignUp(offset, element_size);
                        vertex_attribute_sources[attribute_index] = base_address + loader_config.data_offset + offset;
                        vertex_attribute_strides[attribute_index] = static_cast<u32>(loader_config.byte_count);
                        vertex_attribute_formats[attribute_index] = attribute_config.GetFormat(attribute_index);
                        vertex_attribute_elements[attribute_index] = attribute_config.GetNumElements(attribute_index);
                        vertex_attribute_element_size[attribute_index] = element_size;
                        vertex_attribute_default[attribute_index] = attribute_config.IsDefaultAttribute(attribute_index);
                        offset += attribute_config.GetStride(attribute_index);
                    } else if (attribute_index < 16) {
                        // Attribute ids 12, 13, 14 and 15 signify 4, 8, 12 and 16-byte paddings, respectively
                        offset = Common::AlignUp(offset, 4);
                        offset += (attribute_index - 11) * 4;
                    } else {
                        UNREACHABLE(); // This is truly unreachable due to the number of bits for each component
                    }
                }
            }

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
                        g_debug_context->recorder->MemoryAccessed(texture_data, Pica::Regs::NibblesPerPixel(texture.format) * texture.config.width / 2 * texture.config.height, texture.config.GetPhysicalAddress());
                }
            }

            class {
                /// Combine overlapping and close ranges
                void SimplifyRanges() {
                    for (auto it = ranges.begin(); it != ranges.end(); ++it) {
                        // NOTE: We add 32 to the range end address to make sure "close" ranges are combined, too
                        auto it2 = std::next(it);
                        while (it2 != ranges.end() && it->first + it->second + 32 >= it2->first) {
                            it->second = std::max(it->second, it2->first + it2->second - it->first);
                            it2 = ranges.erase(it2);
                        }
                    }
                }

            public:
                /// Record a particular memory access in the list
                void AddAccess(u32 paddr, u32 size) {
                    // Create new range or extend existing one
                    ranges[paddr] = std::max(ranges[paddr], size);

                    // Simplify ranges...
                    SimplifyRanges();
                }

                /// Map of accessed ranges (mapping start address to range size)
                std::map<u32, u32> ranges;
            } memory_accesses;

            // Simple circular-replacement vertex cache
            // The size has been tuned for optimal balance between hit-rate and the cost of lookup
            const size_t VERTEX_CACHE_SIZE = 32;
            std::array<u16, VERTEX_CACHE_SIZE> vertex_cache_ids;
            std::array<Shader::OutputVertex, VERTEX_CACHE_SIZE> vertex_cache;

            unsigned int vertex_cache_pos = 0;
            vertex_cache_ids.fill(-1);

            Shader::UnitState<false> shader_unit;
            Shader::Setup();

            for (unsigned int index = 0; index < regs.num_vertices; ++index)
            {
                // Indexed rendering doesn't use the start offset
                unsigned int vertex = is_indexed ? (index_u16 ? index_address_16[index] : index_address_8[index]) : (index + regs.vertex_offset);

                // -1 is a common special value used for primitive restart. Since it's unknown if
                // the PICA supports it, and it would mess up the caching, guard against it here.
                ASSERT(vertex != -1);

                bool vertex_cache_hit = false;
                Shader::OutputVertex output;

                if (is_indexed) {
                    if (g_debug_context && Pica::g_debug_context->recorder) {
                        int size = index_u16 ? 2 : 1;
                        memory_accesses.AddAccess(base_address + index_info.offset + size * index, size);
                    }

                    for (unsigned int i = 0; i < VERTEX_CACHE_SIZE; ++i) {
                        if (vertex == vertex_cache_ids[i]) {
                            output = vertex_cache[i];
                            vertex_cache_hit = true;
                            break;
                        }
                    }
                }

                if (!vertex_cache_hit) {
                    // Initialize data for the current vertex
                    Shader::InputVertex input;

                    for (int i = 0; i < num_total_attributes; ++i) {
                        if (vertex_attribute_elements[i] != 0) {
                            // Default attribute values set if array elements have < 4 components. This
                            // is *not* carried over from the default attribute settings even if they're
                            // enabled for this attribute.
                            static const float24 zero = float24::FromFloat32(0.0f);
                            static const float24 one = float24::FromFloat32(1.0f);
                            input.attr[i] = Math::Vec4<float24>(zero, zero, zero, one);

                            // Load per-vertex data from the loader arrays
                            for (unsigned int comp = 0; comp < vertex_attribute_elements[i]; ++comp) {
                                u32 source_addr = vertex_attribute_sources[i] + vertex_attribute_strides[i] * vertex + comp * vertex_attribute_element_size[i];
                                const u8* srcdata = Memory::GetPhysicalPointer(source_addr);

                                if (g_debug_context && Pica::g_debug_context->recorder) {
                                    memory_accesses.AddAccess(source_addr,
                                        (vertex_attribute_formats[i] == Regs::VertexAttributeFormat::FLOAT) ? 4
                                        : (vertex_attribute_formats[i] == Regs::VertexAttributeFormat::SHORT) ? 2 : 1);
                                }

                                const float srcval =
                                    (vertex_attribute_formats[i] == Regs::VertexAttributeFormat::BYTE)  ? *reinterpret_cast<const s8*>(srcdata) :
                                    (vertex_attribute_formats[i] == Regs::VertexAttributeFormat::UBYTE) ? *reinterpret_cast<const u8*>(srcdata) :
                                    (vertex_attribute_formats[i] == Regs::VertexAttributeFormat::SHORT) ? *reinterpret_cast<const s16*>(srcdata) :
                                    *reinterpret_cast<const float*>(srcdata);

                                input.attr[i][comp] = float24::FromFloat32(srcval);
                                LOG_TRACE(HW_GPU, "Loaded component %x of attribute %x for vertex %x (index %x) from 0x%08x + 0x%08x + 0x%04x: %f",
                                    comp, i, vertex, index,
                                    base_address,
                                    vertex_attribute_sources[i] - base_address,
                                    vertex_attribute_strides[i] * vertex + comp * vertex_attribute_element_size[i],
                                    input.attr[i][comp].ToFloat32());
                            }
                        } else if (vertex_attribute_default[i]) {
                            // Load the default attribute if we're configured to do so
                            input.attr[i] = g_state.vs.default_attributes[i];
                            LOG_TRACE(HW_GPU, "Loaded default attribute %x for vertex %x (index %x): (%f, %f, %f, %f)",
                                      i, vertex, index,
                                      input.attr[i][0].ToFloat32(), input.attr[i][1].ToFloat32(),
                                      input.attr[i][2].ToFloat32(), input.attr[i][3].ToFloat32());
                        } else {
                            // TODO(yuriks): In this case, no data gets loaded and the vertex
                            // remains with the last value it had. This isn't currently maintained
                            // as global state, however, and so won't work in Citra yet.
                        }
                    }

                    if (g_debug_context)
                        g_debug_context->OnEvent(DebugContext::Event::VertexLoaded, (void*)&input);

                    // Send to vertex shader
                    output = Shader::Run(shader_unit, input, num_total_attributes);

                    if (is_indexed) {
                        vertex_cache[vertex_cache_pos] = output;
                        vertex_cache_ids[vertex_cache_pos] = vertex;
                        vertex_cache_pos = (vertex_cache_pos + 1) % VERTEX_CACHE_SIZE;
                    }
                }

                // Send to renderer
                using Pica::Shader::OutputVertex;
                auto AddTriangle = [](
                        const OutputVertex& v0, const OutputVertex& v1, const OutputVertex& v2) {
                    VideoCore::g_renderer->Rasterizer()->AddTriangle(v0, v1, v2);
                };

                primitive_assembler.SubmitVertex(output, AddTriangle);
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
        case PICA_REG_INDEX_WORKAROUND(vs.int_uniforms[3], 0x2b4):
        {
            int index = (id - PICA_REG_INDEX_WORKAROUND(vs.int_uniforms[0], 0x2b1));
            auto values = regs.vs.int_uniforms[index];
            g_state.vs.uniforms.i[index] = Math::Vec4<u8>(values.x, values.y, values.z, values.w);
            LOG_TRACE(HW_GPU, "Set integer uniform %d to %02x %02x %02x %02x",
                      index, values.x.Value(), values.y.Value(), values.z.Value(), values.w.Value());
            break;
        }

        case PICA_REG_INDEX_WORKAROUND(vs.uniform_setup.set_value[0], 0x2c1):
        case PICA_REG_INDEX_WORKAROUND(vs.uniform_setup.set_value[1], 0x2c2):
        case PICA_REG_INDEX_WORKAROUND(vs.uniform_setup.set_value[2], 0x2c3):
        case PICA_REG_INDEX_WORKAROUND(vs.uniform_setup.set_value[3], 0x2c4):
        case PICA_REG_INDEX_WORKAROUND(vs.uniform_setup.set_value[4], 0x2c5):
        case PICA_REG_INDEX_WORKAROUND(vs.uniform_setup.set_value[5], 0x2c6):
        case PICA_REG_INDEX_WORKAROUND(vs.uniform_setup.set_value[6], 0x2c7):
        case PICA_REG_INDEX_WORKAROUND(vs.uniform_setup.set_value[7], 0x2c8):
        {
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
                    for (auto i : {0,1,2,3})
                        uniform[3 - i] = float24::FromFloat32(*(float*)(&uniform_write_buffer[i]));
                } else {
                    // TODO: Untested
                    uniform.w = float24::FromRaw(uniform_write_buffer[0] >> 8);
                    uniform.z = float24::FromRaw(((uniform_write_buffer[0] & 0xFF) << 16) | ((uniform_write_buffer[1] >> 16) & 0xFFFF));
                    uniform.y = float24::FromRaw(((uniform_write_buffer[1] & 0xFFFF) << 8) | ((uniform_write_buffer[2] >> 24) & 0xFF));
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
        case PICA_REG_INDEX_WORKAROUND(vs.program.set_word[7], 0x2d3):
        {
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
        case PICA_REG_INDEX_WORKAROUND(vs.swizzle_patterns.set_word[7], 0x2dd):
        {
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
        case PICA_REG_INDEX_WORKAROUND(lighting.lut_data[7], 0x1cf):
        {
            auto& lut_config = regs.lighting.lut_config;

            ASSERT_MSG(lut_config.index < 256, "lut_config.index exceeded maximum value of 255!");

            g_state.lighting.luts[lut_config.type][lut_config.index].raw = value;
            lut_config.index.Assign(lut_config.index + 1);
            break;
        }

        default:
            break;
    }

    VideoCore::g_renderer->Rasterizer()->NotifyPicaRegisterChanged(id);

    if (g_debug_context)
        g_debug_context->OnEvent(DebugContext::Event::PicaCommandProcessed, reinterpret_cast<void*>(&id));
}

void ProcessCommandList(const u32* list, u32 size) {
    g_state.cmd_list.head_ptr = g_state.cmd_list.current_ptr = list;
    g_state.cmd_list.length = size / sizeof(u32);

    while (g_state.cmd_list.current_ptr < g_state.cmd_list.head_ptr + g_state.cmd_list.length) {

        // Align read pointer to 8 bytes
        if ((g_state.cmd_list.head_ptr - g_state.cmd_list.current_ptr) % 2 != 0)
            ++g_state.cmd_list.current_ptr;

        u32 value = *g_state.cmd_list.current_ptr++;
        const CommandHeader header = { *g_state.cmd_list.current_ptr++ };

        WritePicaReg(header.cmd_id, value, header.parameter_mask);

        for (unsigned i = 0; i < header.extra_data_length; ++i) {
            u32 cmd = header.cmd_id + (header.group_commands ? i + 1 : 0);
            WritePicaReg(cmd, *g_state.cmd_list.current_ptr++, header.parameter_mask);
         }
    }
}

} // namespace

} // namespace
