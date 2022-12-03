#pragma once

#include "common/common_types.h"

namespace GDBStub {

struct PackedGdbHioRequest {
    char magic[4]; // "GDB\x00"
    u32 version;

    // Request
    char function_name[16 + 1];
    char param_format[8 + 1];

    u64 parameters[8];
    size_t string_lengths[8];

    // Return
    s64 retval;
    int gdb_errno;
    bool ctrl_c;
};

void SetHioRequest(const VAddr address);

bool HandleHioRequest(const u8* const command_buffer, const u32 command_length);

bool HasHioRequest();

std::string BuildHioReply();

} // namespace GDBStub
