// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>

#include "common/assert.h"
#include "common/file_util.h"
#include "common/logging/log.h"

#include "recorder.h"

namespace CiTrace {

Recorder::Recorder(u32* gpu_registers,     u32 gpu_registers_size,
                   u32* lcd_registers,     u32 lcd_registers_size,
                   u32* pica_registers,    u32 pica_registers_size,
                   u32* vs_program_binary, u32 vs_program_binary_size,
                   u32* vs_swizzle_data,   u32 vs_swizzle_data_size,
                   u32* vs_float_uniforms, u32 vs_float_uniforms_size,
                   u32* gs_program_binary, u32 gs_program_binary_size,
                   u32* gs_swizzle_data,   u32 gs_swizzle_data_size,
                   u32* gs_float_uniforms, u32 gs_float_uniforms_size)
    : gpu_registers(gpu_registers, gpu_registers + gpu_registers_size),
      lcd_registers(lcd_registers, lcd_registers + lcd_registers_size),
      pica_registers(pica_registers, pica_registers + pica_registers_size),
      vs_program_binary(vs_program_binary, vs_program_binary + vs_program_binary_size),
      vs_swizzle_data(vs_swizzle_data, vs_swizzle_data + vs_swizzle_data_size),
      vs_float_uniforms(vs_float_uniforms, vs_float_uniforms + vs_float_uniforms_size),
      gs_program_binary(gs_program_binary, gs_program_binary + gs_program_binary_size),
      gs_swizzle_data(gs_swizzle_data, gs_swizzle_data + gs_swizzle_data_size),
      gs_float_uniforms(gs_float_uniforms, gs_float_uniforms + gs_float_uniforms_size) {

}

void Recorder::Finish(const std::string& filename) {
    // Setup CiTrace header
    CTHeader header;
    std::memcpy(header.magic, CTHeader::ExpectedMagicWord(), 4);
    header.version = CTHeader::ExpectedVersion();
    header.header_size = sizeof(CTHeader);

    // Calculate file offsets
    auto& initial = header.initial_state_offsets;

    initial.gpu_registers_size     = gpu_registers.size();
    initial.lcd_registers_size     = lcd_registers.size();
    initial.pica_registers_size    = pica_registers.size();
    initial.vs_program_binary_size = vs_program_binary.size();
    initial.vs_swizzle_data_size   = vs_swizzle_data.size();
    initial.vs_float_uniforms_size = vs_float_uniforms.size();
    initial.gs_program_binary_size = gs_program_binary.size();
    initial.gs_swizzle_data_size   = gs_swizzle_data.size();
    initial.gs_float_uniforms_size = gs_float_uniforms.size();
    header.stream_size             = stream.size();

    initial.gpu_registers     = sizeof(header);
    initial.lcd_registers     = initial.gpu_registers     + initial.gpu_registers_size * sizeof(u32);
    initial.pica_registers    = initial.lcd_registers     + initial.lcd_registers_size * sizeof(u32);;
    initial.vs_program_binary = initial.pica_registers    + initial.pica_registers_size * sizeof(u32);
    initial.vs_swizzle_data   = initial.vs_program_binary + initial.vs_program_binary_size * sizeof(u32);
    initial.vs_float_uniforms = initial.vs_swizzle_data   + initial.vs_swizzle_data_size * sizeof(u32);
    initial.gs_program_binary = initial.vs_float_uniforms + initial.vs_float_uniforms_size * sizeof(u32);
    initial.gs_swizzle_data   = initial.gs_program_binary + initial.gs_program_binary_size * sizeof(u32);
    initial.gs_float_uniforms = initial.gs_swizzle_data   + initial.gs_swizzle_data_size * sizeof(u32);
    header.stream_offset      = initial.gs_float_uniforms + initial.gs_float_uniforms_size * sizeof(u32);

    // Iterate through stream elements, update relevant stream element data
    for (auto& stream_element : stream) {
        switch (stream_element.data.type) {
        case MemoryLoad:
        {
            auto& file_offset = memory_regions[stream_element.hash];
            if (!stream_element.uses_existing_data) {
                file_offset = header.stream_offset;
            }
            stream_element.data.memory_load.file_offset = file_offset;
            break;
        }

        default:
            // Other commands don't use any extra data
            DEBUG_ASSERT(stream_element.extra_data.size() == 0);
            break;
        }
        header.stream_offset += stream_element.extra_data.size();
    }

    try {
        // Open file and write header
        FileUtil::IOFile file(filename, "wb");
        size_t written = file.WriteObject(header);
        if (written != 1 || file.Tell() != initial.gpu_registers)
            throw "Failed to write header";

        // Write initial state
        written = file.WriteArray(gpu_registers.data(), gpu_registers.size());
        if (written != gpu_registers.size() || file.Tell() != initial.lcd_registers)
            throw "Failed to write GPU registers";

        written = file.WriteArray(lcd_registers.data(), lcd_registers.size());
        if (written != lcd_registers.size() || file.Tell() != initial.pica_registers)
            throw "Failed to write LCD registers";

        written = file.WriteArray(pica_registers.data(), pica_registers.size());
        if (written != pica_registers.size() || file.Tell() != initial.vs_program_binary)
            throw "Failed to write Pica registers";

        written = file.WriteArray(vs_program_binary.data(), vs_program_binary.size());
        if (written != vs_program_binary.size() || file.Tell() != initial.vs_swizzle_data)
            throw "Failed to write vertex shader program binary";

        written = file.WriteArray(vs_swizzle_data.data(), vs_swizzle_data.size());
        if (written != vs_swizzle_data.size() || file.Tell() != initial.vs_float_uniforms)
            throw "Failed to write vertex shader swizzle data";

        written = file.WriteArray(vs_float_uniforms.data(), vs_float_uniforms.size());
        if (written != vs_float_uniforms.size() || file.Tell() != initial.gs_program_binary)
            throw "Failed to write vertex shader float uniforms";

        written = file.WriteArray(gs_program_binary.data(), gs_program_binary.size());
        if (written != gs_program_binary.size() || file.Tell() != initial.gs_swizzle_data)
            throw "Failed to write geomtry shader program binary";

        written = file.WriteArray(gs_swizzle_data.data(), gs_swizzle_data.size());
        if (written != gs_swizzle_data.size() || file.Tell() != initial.gs_float_uniforms)
            throw "Failed to write geometry shader swizzle data";

        written = file.WriteArray(gs_float_uniforms.data(), gs_float_uniforms.size());
        if (written != gs_float_uniforms.size() || file.Tell() != initial.gs_float_uniforms + sizeof(u32) * initial.gs_float_uniforms_size)
            throw "Failed to write geometry shader float uniforms";

        // Iterate through stream elements, write "extra data"
        for (const auto& stream_element : stream) {
            if (stream_element.extra_data.size() == 0)
                continue;

            written = file.WriteBytes(stream_element.extra_data.data(), stream_element.extra_data.size());
            if (written != stream_element.extra_data.size())
                throw "Failed to write extra data";
        }

        if (file.Tell() != header.stream_offset)
            throw "Unexpected end of extra data";

        // Write actual stream elements
        for (const auto& stream_element : stream) {
            if (1 != file.WriteObject(stream_element.data))
                throw "Failed to write stream element";
        }
    } catch(const char* str) {
        LOG_ERROR(HW_GPU, "Writing CiTrace file failed: %s", str);
    }
}

void Recorder::FrameFinished() {
    stream.push_back( { FrameMarker } );
}

void Recorder::MemoryAccessed(const u8* data, u32 size, u32 physical_address) {
    StreamElement element = { MemoryLoad };
    element.data.memory_load.size = size;
    element.data.memory_load.physical_address = physical_address;

    // Compute hash over given memory region to check if the contents are already stored internally
    boost::crc_32_type result;
    result.process_bytes(data, size);
    element.hash = result.checksum();

    element.uses_existing_data = (memory_regions.find(element.hash) != memory_regions.end());
    if (!element.uses_existing_data) {
        element.extra_data.resize(size);
        memcpy(element.extra_data.data(), data, size);
        memory_regions.insert({element.hash, 0}); // file offset will be initialized in Finish()
    }

    stream.push_back(element);
}

template<typename T>
void Recorder::RegisterWritten(u32 physical_address, T value) {
    StreamElement element = { RegisterWrite };
    element.data.register_write.size = (sizeof(T) == 1) ? CTRegisterWrite::SIZE_8
                                     : (sizeof(T) == 2) ? CTRegisterWrite::SIZE_16
                                     : (sizeof(T) == 4) ? CTRegisterWrite::SIZE_32
                                     :                    CTRegisterWrite::SIZE_64;
    element.data.register_write.physical_address = physical_address;
    element.data.register_write.value = value;

    stream.push_back(element);
}

template void Recorder::RegisterWritten(u32,u8);
template void Recorder::RegisterWritten(u32,u16);
template void Recorder::RegisterWritten(u32,u32);
template void Recorder::RegisterWritten(u32,u64);

}
