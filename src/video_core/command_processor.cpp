// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/range/algorithm/fill.hpp>

#include "common/profiler.h"

#include "clipper.h"
#include "command_processor.h"
#include "math.h"
#include "pica.h"
#include "primitive_assembly.h"
#include "vertex_shader.h"
#include "core/hle/service/gsp_gpu.h"
#include "core/hw/gpu.h"

#include "debug_utils/debug_utils.h"

namespace Pica {

Regs registers;

namespace CommandProcessor {

static int float_regs_counter = 0;

static u32 uniform_write_buffer[4];

static int default_attr_counter = 0;

static u32 default_attr_write_buffer[3];

Common::Profiling::TimingCategory category_drawing("Drawing");

static inline void WritePicaReg(u32 id, u32 value, u32 mask) {

    if (id >= registers.NumIds())
        return;

    // If we're skipping this frame, only allow trigger IRQ
    if (GPU::g_skip_frame && id != PICA_REG_INDEX(trigger_irq))
        return;

    // TODO: Figure out how register masking acts on e.g. vs_uniform_setup.set_value
    u32 old_value = registers[id];
    registers[id] = (old_value & ~mask) | (value & mask);

    if (g_debug_context)
        g_debug_context->OnEvent(DebugContext::Event::CommandLoaded, reinterpret_cast<void*>(&id));

    DebugUtils::OnPicaRegWrite(id, registers[id]);

    switch(id) {
        // Trigger IRQ
        case PICA_REG_INDEX(trigger_irq):
            GSP_GPU::SignalInterrupt(GSP_GPU::InterruptId::P3D);
            return;

        // It seems like these trigger vertex rendering
        case PICA_REG_INDEX(trigger_draw):
        case PICA_REG_INDEX(trigger_draw_indexed):
        {
            Common::Profiling::ScopeTimer scope_timer(category_drawing);

            DebugUtils::DumpTevStageConfig(registers.GetTevStages());

            if (g_debug_context)
                g_debug_context->OnEvent(DebugContext::Event::IncomingPrimitiveBatch, nullptr);

            const auto& attribute_config = registers.vertex_attributes;
            const u32 base_address = attribute_config.GetPhysicalBaseAddress();

            // Information about internal vertex attributes
            u32 vertex_attribute_sources[16];
            boost::fill(vertex_attribute_sources, 0xdeadbeef);
            u32 vertex_attribute_strides[16];
            Regs::VertexAttributeFormat vertex_attribute_formats[16];

            u32 vertex_attribute_elements[16];
            u32 vertex_attribute_element_size[16];

            // Setup attribute data from loaders
            for (int loader = 0; loader < 12; ++loader) {
                const auto& loader_config = attribute_config.attribute_loaders[loader];

                u32 load_address = base_address + loader_config.data_offset;

                // TODO: What happens if a loader overwrites a previous one's data?
                for (unsigned component = 0; component < loader_config.component_count; ++component) {
                    u32 attribute_index = loader_config.GetComponent(component);
                    vertex_attribute_sources[attribute_index] = load_address;
                    vertex_attribute_strides[attribute_index] = static_cast<u32>(loader_config.byte_count);
                    vertex_attribute_formats[attribute_index] = attribute_config.GetFormat(attribute_index);
                    vertex_attribute_elements[attribute_index] = attribute_config.GetNumElements(attribute_index);
                    vertex_attribute_element_size[attribute_index] = attribute_config.GetElementSizeInBytes(attribute_index);
                    load_address += attribute_config.GetStride(attribute_index);
                }
            }

            // Load vertices
            bool is_indexed = (id == PICA_REG_INDEX(trigger_draw_indexed));

            const auto& index_info = registers.index_array;
            const u8* index_address_8 = Memory::GetPhysicalPointer(base_address + index_info.offset);
            const u16* index_address_16 = (u16*)index_address_8;
            bool index_u16 = index_info.format != 0;

            DebugUtils::GeometryDumper geometry_dumper;
            PrimitiveAssembler<VertexShader::OutputVertex> clipper_primitive_assembler(registers.triangle_topology.Value());
            PrimitiveAssembler<DebugUtils::GeometryDumper::Vertex> dumping_primitive_assembler(registers.triangle_topology.Value());

            for (unsigned int index = 0; index < registers.num_vertices; ++index)
            {
                unsigned int vertex = is_indexed ? (index_u16 ? index_address_16[index] : index_address_8[index]) : index;

                if (is_indexed) {
                    // TODO: Implement some sort of vertex cache!
                }

                // Initialize data for the current vertex
                VertexShader::InputVertex input;

                // Load a debugging token to check whether this gets loaded by the running
                // application or not.
                static const float24 debug_token = float24::FromRawFloat24(0x00abcdef);
                input.attr[0].w = debug_token;

                for (int i = 0; i < attribute_config.GetNumTotalAttributes(); ++i) {
                    if (attribute_config.IsDefaultAttribute(i)) {
                        input.attr[i] = VertexShader::GetDefaultAttribute(i);
                        LOG_TRACE(HW_GPU, "Loaded default attribute %x for vertex %x (index %x): (%f, %f, %f, %f)",
                                  i, vertex, index,
                                  input.attr[i][0].ToFloat32(), input.attr[i][1].ToFloat32(),
                                  input.attr[i][2].ToFloat32(), input.attr[i][3].ToFloat32());
                    } else {
                        for (unsigned int comp = 0; comp < vertex_attribute_elements[i]; ++comp) {
                            const u8* srcdata = Memory::GetPhysicalPointer(vertex_attribute_sources[i] + vertex_attribute_strides[i] * vertex + comp * vertex_attribute_element_size[i]);

                            const float srcval = (vertex_attribute_formats[i] == Regs::VertexAttributeFormat::BYTE) ? *(s8*)srcdata :
                                (vertex_attribute_formats[i] == Regs::VertexAttributeFormat::UBYTE) ? *(u8*)srcdata :
                                (vertex_attribute_formats[i] == Regs::VertexAttributeFormat::SHORT) ? *(s16*)srcdata :
                                *(float*)srcdata;

                            input.attr[i][comp] = float24::FromFloat32(srcval);
                            LOG_TRACE(HW_GPU, "Loaded component %x of attribute %x for vertex %x (index %x) from 0x%08x + 0x%08lx + 0x%04lx: %f",
                                      comp, i, vertex, index,
                                      attribute_config.GetPhysicalBaseAddress(),
                                      vertex_attribute_sources[i] - base_address,
                                      vertex_attribute_strides[i] * vertex + comp * vertex_attribute_element_size[i],
                                      input.attr[i][comp].ToFloat32());
                        }
                    }
                }

                // HACK: Some games do not initialize the vertex position's w component. This leads
                //       to critical issues since it messes up perspective division. As a
                //       workaround, we force the fourth component to 1.0 if we find this to be the
                //       case.
                //       To do this, we additionally have to assume that the first input attribute
                //       is the vertex position, since there's no information about this other than
                //       the empiric observation that this is usually the case.
                if (input.attr[0].w == debug_token)
                    input.attr[0].w = float24::FromFloat32(1.0);

                if (g_debug_context)
                    g_debug_context->OnEvent(DebugContext::Event::VertexLoaded, (void*)&input);

                // NOTE: When dumping geometry, we simply assume that the first input attribute
                //       corresponds to the position for now.
                DebugUtils::GeometryDumper::Vertex dumped_vertex = {
                    input.attr[0][0].ToFloat32(), input.attr[0][1].ToFloat32(), input.attr[0][2].ToFloat32()
                };
                using namespace std::placeholders;
                dumping_primitive_assembler.SubmitVertex(dumped_vertex,
                                                         std::bind(&DebugUtils::GeometryDumper::AddTriangle,
                                                                   &geometry_dumper, _1, _2, _3));

                // Send to vertex shader
                VertexShader::OutputVertex output = VertexShader::RunShader(input, attribute_config.GetNumTotalAttributes());

                if (is_indexed) {
                    // TODO: Add processed vertex to vertex cache!
                }

                // Send to triangle clipper
                clipper_primitive_assembler.SubmitVertex(output, Clipper::ProcessTriangle);
            }
            geometry_dumper.Dump();

            if (g_debug_context)
                g_debug_context->OnEvent(DebugContext::Event::FinishedPrimitiveBatch, nullptr);

            break;
        }

        case PICA_REG_INDEX(vs_bool_uniforms):
            for (unsigned i = 0; i < 16; ++i)
                VertexShader::GetBoolUniform(i) = (registers.vs_bool_uniforms.Value() & (1 << i)) != 0;

            break;

        case PICA_REG_INDEX_WORKAROUND(vs_int_uniforms[0], 0x2b1):
        case PICA_REG_INDEX_WORKAROUND(vs_int_uniforms[1], 0x2b2):
        case PICA_REG_INDEX_WORKAROUND(vs_int_uniforms[2], 0x2b3):
        case PICA_REG_INDEX_WORKAROUND(vs_int_uniforms[3], 0x2b4):
        {
            int index = (id - PICA_REG_INDEX_WORKAROUND(vs_int_uniforms[0], 0x2b1));
            auto values = registers.vs_int_uniforms[index];
            VertexShader::GetIntUniform(index) = Math::Vec4<u8>(values.x, values.y, values.z, values.w);
            LOG_TRACE(HW_GPU, "Set integer uniform %d to %02x %02x %02x %02x",
                      index, values.x.Value(), values.y.Value(), values.z.Value(), values.w.Value());
            break;
        }

        case PICA_REG_INDEX_WORKAROUND(vs_uniform_setup.set_value[0], 0x2c1):
        case PICA_REG_INDEX_WORKAROUND(vs_uniform_setup.set_value[1], 0x2c2):
        case PICA_REG_INDEX_WORKAROUND(vs_uniform_setup.set_value[2], 0x2c3):
        case PICA_REG_INDEX_WORKAROUND(vs_uniform_setup.set_value[3], 0x2c4):
        case PICA_REG_INDEX_WORKAROUND(vs_uniform_setup.set_value[4], 0x2c5):
        case PICA_REG_INDEX_WORKAROUND(vs_uniform_setup.set_value[5], 0x2c6):
        case PICA_REG_INDEX_WORKAROUND(vs_uniform_setup.set_value[6], 0x2c7):
        case PICA_REG_INDEX_WORKAROUND(vs_uniform_setup.set_value[7], 0x2c8):
        {
            auto& uniform_setup = registers.vs_uniform_setup;

            // TODO: Does actual hardware indeed keep an intermediate buffer or does
            //       it directly write the values?
            uniform_write_buffer[float_regs_counter++] = value;

            // Uniforms are written in a packed format such that four float24 values are encoded in
            // three 32-bit numbers. We write to internal memory once a full such vector is
            // written.
            if ((float_regs_counter >= 4 && uniform_setup.IsFloat32()) ||
                (float_regs_counter >= 3 && !uniform_setup.IsFloat32())) {
                float_regs_counter = 0;

                auto& uniform = VertexShader::GetFloatUniform(uniform_setup.index);

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
                    uniform.w = float24::FromRawFloat24(uniform_write_buffer[0] >> 8);
                    uniform.z = float24::FromRawFloat24(((uniform_write_buffer[0] & 0xFF)<<16) | ((uniform_write_buffer[1] >> 16) & 0xFFFF));
                    uniform.y = float24::FromRawFloat24(((uniform_write_buffer[1] & 0xFFFF)<<8) | ((uniform_write_buffer[2] >> 24) & 0xFF));
                    uniform.x = float24::FromRawFloat24(uniform_write_buffer[2] & 0xFFFFFF);
                }

                LOG_TRACE(HW_GPU, "Set uniform %x to (%f %f %f %f)", (int)uniform_setup.index,
                          uniform.x.ToFloat32(), uniform.y.ToFloat32(), uniform.z.ToFloat32(),
                          uniform.w.ToFloat32());

                // TODO: Verify that this actually modifies the register!
                uniform_setup.index = uniform_setup.index + 1;
            }
            break;
        }
        
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

                auto& setup = registers.vs_default_attributes_setup;

                if (setup.index >= 16) {
                    LOG_ERROR(HW_GPU, "Invalid VS default attribute index %d", (int)setup.index);
                    break;
                }

                Math::Vec4<float24>& attribute = VertexShader::GetDefaultAttribute(setup.index);
                
                // NOTE: The destination component order indeed is "backwards"
                attribute.w = float24::FromRawFloat24(default_attr_write_buffer[0] >> 8);
                attribute.z = float24::FromRawFloat24(((default_attr_write_buffer[0] & 0xFF) << 16) | ((default_attr_write_buffer[1] >> 16) & 0xFFFF));
                attribute.y = float24::FromRawFloat24(((default_attr_write_buffer[1] & 0xFFFF) << 8) | ((default_attr_write_buffer[2] >> 24) & 0xFF));
                attribute.x = float24::FromRawFloat24(default_attr_write_buffer[2] & 0xFFFFFF);

                LOG_TRACE(HW_GPU, "Set default VS attribute %x to (%f %f %f %f)", (int)setup.index,
                          attribute.x.ToFloat32(), attribute.y.ToFloat32(), attribute.z.ToFloat32(),
                          attribute.w.ToFloat32());

                // TODO: Verify that this actually modifies the register!
                setup.index = setup.index + 1;
            }
            break;
        }

        // Load shader program code
        case PICA_REG_INDEX_WORKAROUND(vs_program.set_word[0], 0x2cc):
        case PICA_REG_INDEX_WORKAROUND(vs_program.set_word[1], 0x2cd):
        case PICA_REG_INDEX_WORKAROUND(vs_program.set_word[2], 0x2ce):
        case PICA_REG_INDEX_WORKAROUND(vs_program.set_word[3], 0x2cf):
        case PICA_REG_INDEX_WORKAROUND(vs_program.set_word[4], 0x2d0):
        case PICA_REG_INDEX_WORKAROUND(vs_program.set_word[5], 0x2d1):
        case PICA_REG_INDEX_WORKAROUND(vs_program.set_word[6], 0x2d2):
        case PICA_REG_INDEX_WORKAROUND(vs_program.set_word[7], 0x2d3):
        {
            VertexShader::SubmitShaderMemoryChange(registers.vs_program.offset, value);
            registers.vs_program.offset++;
            break;
        }

        // Load swizzle pattern data
        case PICA_REG_INDEX_WORKAROUND(vs_swizzle_patterns.set_word[0], 0x2d6):
        case PICA_REG_INDEX_WORKAROUND(vs_swizzle_patterns.set_word[1], 0x2d7):
        case PICA_REG_INDEX_WORKAROUND(vs_swizzle_patterns.set_word[2], 0x2d8):
        case PICA_REG_INDEX_WORKAROUND(vs_swizzle_patterns.set_word[3], 0x2d9):
        case PICA_REG_INDEX_WORKAROUND(vs_swizzle_patterns.set_word[4], 0x2da):
        case PICA_REG_INDEX_WORKAROUND(vs_swizzle_patterns.set_word[5], 0x2db):
        case PICA_REG_INDEX_WORKAROUND(vs_swizzle_patterns.set_word[6], 0x2dc):
        case PICA_REG_INDEX_WORKAROUND(vs_swizzle_patterns.set_word[7], 0x2dd):
        {
            VertexShader::SubmitSwizzleDataChange(registers.vs_swizzle_patterns.offset, value);
            registers.vs_swizzle_patterns.offset++;
            break;
        }

        default:
            break;
    }

    if (g_debug_context)
        g_debug_context->OnEvent(DebugContext::Event::CommandProcessed, reinterpret_cast<void*>(&id));
}

static std::ptrdiff_t ExecuteCommandBlock(const u32* first_command_word) {
    const CommandHeader& header = *(const CommandHeader*)(&first_command_word[1]);

    u32* read_pointer = (u32*)first_command_word;

    const u32 write_mask = ((header.parameter_mask & 0x1) ? (0xFFu <<  0) : 0u) |
                           ((header.parameter_mask & 0x2) ? (0xFFu <<  8) : 0u) |
                           ((header.parameter_mask & 0x4) ? (0xFFu << 16) : 0u) |
                           ((header.parameter_mask & 0x8) ? (0xFFu << 24) : 0u);

    WritePicaReg(header.cmd_id, *read_pointer, write_mask);
    read_pointer += 2;

    for (unsigned int i = 1; i < 1+header.extra_data_length; ++i) {
        u32 cmd = header.cmd_id + ((header.group_commands) ? i : 0);
        WritePicaReg(cmd, *read_pointer, write_mask);
        ++read_pointer;
    }

    // align read pointer to 8 bytes
    if ((first_command_word - read_pointer) % 2)
        ++read_pointer;

    return read_pointer - first_command_word;
}

void ProcessCommandList(const u32* list, u32 size) {
    u32* read_pointer = (u32*)list;
    u32 list_length = size / sizeof(u32);

    while (read_pointer < list + list_length) {
        read_pointer += ExecuteCommandBlock(read_pointer);
    }
}

} // namespace

} // namespace
