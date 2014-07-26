// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "pica.h"
#include "command_processor.h"
#include "math.h"


namespace Pica {

Regs registers;

namespace CommandProcessor {

static inline void WritePicaReg(u32 id, u32 value) {
    u32 old_value = registers[id];
    registers[id] = value;

    switch(id) {
        // It seems like these trigger vertex rendering
        case PICA_REG_INDEX(trigger_draw):
        case PICA_REG_INDEX(trigger_draw_indexed):
        {
            const auto& attribute_config = registers.vertex_attributes;
            const u8* const base_address = Memory::GetPointer(attribute_config.GetBaseAddress());

            // Information about internal vertex attributes
            const u8* vertex_attribute_sources[16];
            u32 vertex_attribute_strides[16];
            u32 vertex_attribute_formats[16];
            u32 vertex_attribute_elements[16];
            u32 vertex_attribute_element_size[16];

            // Setup attribute data from loaders
            for (int loader = 0; loader < 12; ++loader) {
                const auto& loader_config = attribute_config.attribute_loaders[loader];

                const u8* load_address = base_address + loader_config.data_offset;

                // TODO: What happens if a loader overwrites a previous one's data?
                for (int component = 0; component < loader_config.component_count; ++component) {
                    u32 attribute_index = loader_config.GetComponent(component);
                    vertex_attribute_sources[attribute_index] = load_address;
                    vertex_attribute_strides[attribute_index] = loader_config.byte_count;
                    vertex_attribute_formats[attribute_index] = (u32)attribute_config.GetFormat(attribute_index);
                    vertex_attribute_elements[attribute_index] = attribute_config.GetNumElements(attribute_index);
                    vertex_attribute_element_size[attribute_index] = attribute_config.GetElementSizeInBytes(attribute_index);
                    load_address += attribute_config.GetStride(attribute_index);
                }
            }

            // Load vertices
            bool is_indexed = (id == PICA_REG_INDEX(trigger_draw_indexed));

            const auto& index_info = registers.index_array;
            const u8* index_address_8 = (u8*)base_address + index_info.offset;
            const u16* index_address_16 = (u16*)index_address_8;
            bool index_u16 = (bool)index_info.format;

            for (int index = 0; index < registers.num_vertices; ++index)
            {
                int vertex = is_indexed ? (index_u16 ? index_address_16[index] : index_address_8[index]) : index;

                if (is_indexed) {
                    // TODO: Implement some sort of vertex cache!
                }

                // Initialize data for the current vertex
                struct {
                    Math::Vec4<float24> attr[16];
                } input;

                for (int i = 0; i < attribute_config.GetNumTotalAttributes(); ++i) {
                    for (int comp = 0; comp < vertex_attribute_elements[i]; ++comp) {
                        const u8* srcdata = vertex_attribute_sources[i] + vertex_attribute_strides[i] * vertex + comp * vertex_attribute_element_size[i];
                        const float srcval = (vertex_attribute_formats[i] == 0) ? *(s8*)srcdata :
                                             (vertex_attribute_formats[i] == 1) ? *(u8*)srcdata :
                                             (vertex_attribute_formats[i] == 2) ? *(s16*)srcdata :
                                                                                  *(float*)srcdata;
                        input.attr[i][comp] = float24::FromFloat32(srcval);
                        DEBUG_LOG(GPU, "Loaded component %x of attribute %x for vertex %x (index %x) from 0x%08x + 0x%08x + 0x%04x: %f",
                                  comp, i, vertex, index,
                                  attribute_config.GetBaseAddress(),
                                  vertex_attribute_sources[i] - base_address,
                                  srcdata - vertex_attribute_sources[i],
                                  input.attr[i][comp].ToFloat32());
                    }
                }
                // TODO: Run vertex data through vertex shader

                if (is_indexed) {
                    // TODO: Add processed vertex to vertex cache!
                }

                // TODO: Submit vertex to primitive assembly
            }
            break;
        }

        default:
            break;
    }
}

static std::ptrdiff_t ExecuteCommandBlock(const u32* first_command_word) {
    const CommandHeader& header = *(const CommandHeader*)(&first_command_word[1]);

    u32* read_pointer = (u32*)first_command_word;

    // TODO: Take parameter mask into consideration!

    WritePicaReg(header.cmd_id, *read_pointer);
    read_pointer += 2;

    for (int i = 1; i < 1+header.extra_data_length; ++i) {
        u32 cmd = header.cmd_id + ((header.group_commands) ? i : 0);
        WritePicaReg(cmd, *read_pointer);
        ++read_pointer;
    }

    // align read pointer to 8 bytes
    if ((first_command_word - read_pointer) % 2)
        ++read_pointer;

    return read_pointer - first_command_word;
}

void ProcessCommandList(const u32* list, u32 size) {
    u32* read_pointer = (u32*)list;

    while (read_pointer < list + size) {
        read_pointer += ExecuteCommandBlock(read_pointer);
    }
}

} // namespace

} // namespace
