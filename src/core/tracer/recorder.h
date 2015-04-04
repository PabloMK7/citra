// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>
#include <vector>

#include <boost/crc.hpp>

#include "common/common_types.h"

#include "tracer.h"

namespace CiTrace {

class Recorder {
public:
    /**
     * Recorder constructor
     * @param vs_float_uniforms Pointer to an array of 32-bit-aligned 24-bit floating point values.
     */
    Recorder(u32* gpu_registers,     u32 gpu_registers_size,
             u32* lcd_registers,     u32 lcd_registers_size,
             u32* pica_registers,    u32 pica_registers_size,
             u32* vs_program_binary, u32 vs_program_binary_size,
             u32* vs_swizzle_data,   u32 vs_swizzle_data_size,
             u32* vs_float_uniforms, u32 vs_float_uniforms_size,
             u32* gs_program_binary, u32 gs_program_binary_size,
             u32* gs_swizzle_data,   u32 gs_swizzle_data_size,
             u32* gs_float_uniforms, u32 gs_float_uniforms_size);

    /// Finish recording of this Citrace and save it using the given filename.
    void Finish(const std::string& filename);

    /// Mark end of a frame
    void FrameFinished();

    /**
     * Store a copy of the given memory range in the recording.
     * @note Use this whenever the GPU is about to access a particular memory region.
     * @note The implementation will make sure to minimize redundant memory updates.
     */
    void MemoryAccessed(const u8* data, u32 size, u32 physical_address);

    /**
     * Record a register write.
     * @note Use this whenever a GPU-related MMIO register has been written to.
     */
    template<typename T>
    void RegisterWritten(u32 physical_address, T value);

private:
    // Initial state of recording start
    std::vector<u32> gpu_registers;
    std::vector<u32> lcd_registers;
    std::vector<u32> pica_registers;
    std::vector<u32> vs_program_binary;
    std::vector<u32> vs_swizzle_data;
    std::vector<u32> vs_float_uniforms;
    std::vector<u32> gs_program_binary;
    std::vector<u32> gs_swizzle_data;
    std::vector<u32> gs_float_uniforms;

    // Command stream
    struct StreamElement {
        CTStreamElement data;

        /**
          * Extra data to store along "core" data.
          * This is e.g. used for data used in MemoryUpdates.
          */
        std::vector<u8> extra_data;

        /// Optional CRC hash (e.g. for hashing memory regions)
        boost::crc_32_type::value_type hash;

        /// If true, refer to data already written to the output file instead of extra_data
        bool uses_existing_data;
    };

    std::vector<StreamElement> stream;

    /**
     * Internal cache which maps hashes of memory contents to file offsets at which those memory
     * contents are stored.
     */
    std::unordered_map<boost::crc_32_type::value_type /*hash*/, u32 /*file_offset*/> memory_regions;
};

} // namespace
