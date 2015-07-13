// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstdint>

namespace CiTrace {

// NOTE: Things are stored in little-endian

#pragma pack(1)

struct CTHeader {
    static const char* ExpectedMagicWord() {
        return "CiTr";
    }

    static uint32_t ExpectedVersion() {
        return 1;
    }

    char magic[4];
    uint32_t version;
    uint32_t header_size;

    struct {
        // NOTE: Register range sizes are technically hardware-constants, but the actual limits
        // aren't known. Hence we store the presumed limits along the offsets.
        // Sizes are given in uint32_t units.
        uint32_t gpu_registers;
        uint32_t gpu_registers_size;
        uint32_t lcd_registers;
        uint32_t lcd_registers_size;
        uint32_t pica_registers;
        uint32_t pica_registers_size;
        uint32_t default_attributes;
        uint32_t default_attributes_size;
        uint32_t vs_program_binary;
        uint32_t vs_program_binary_size;
        uint32_t vs_swizzle_data;
        uint32_t vs_swizzle_data_size;
        uint32_t vs_float_uniforms;
        uint32_t vs_float_uniforms_size;
        uint32_t gs_program_binary;
        uint32_t gs_program_binary_size;
        uint32_t gs_swizzle_data;
        uint32_t gs_swizzle_data_size;
        uint32_t gs_float_uniforms;
        uint32_t gs_float_uniforms_size;

        // Other things we might want to store here:
        // - Initial framebuffer data, maybe even a full copy of FCRAM/VRAM
        // - Lookup tables for fragment lighting
        // - Lookup tables for procedural textures
    } initial_state_offsets;

    uint32_t stream_offset;
    uint32_t stream_size;
};

enum CTStreamElementType : uint32_t {
    FrameMarker   = 0xE1,
    MemoryLoad    = 0xE2,
    RegisterWrite = 0xE3,
};

struct CTMemoryLoad {
    uint32_t file_offset;
    uint32_t size;
    uint32_t physical_address;
    uint32_t pad;
};

struct CTRegisterWrite {
    uint32_t physical_address;

    enum : uint32_t {
        SIZE_8  = 0xD1,
        SIZE_16 = 0xD2,
        SIZE_32 = 0xD3,
        SIZE_64 = 0xD4
    } size;

    // TODO: Make it clearer which bits of this member are used for sizes other than 32 bits
    uint64_t value;
};

struct CTStreamElement {
    CTStreamElementType type;

    union {
        CTMemoryLoad memory_load;
        CTRegisterWrite register_write;
    };
};

#pragma pack()

}
