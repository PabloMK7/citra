// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "common/common_types.h"

namespace GDBStub {

/**
 * A request from a debugged application to perform some I/O with the GDB client.
 * This structure is also used to encode the reply back to the application.
 *
 * Based on the Rosalina implementation:
 * https://github.com/LumaTeam/Luma3DS/blob/master/sysmodules/rosalina/include/gdb.h#L46C27-L62
 */
struct PackedGdbHioRequest {
    char magic[4]; // "GDB\0"
    u32 version;

    // Request
    char function_name[16 + 1];
    char param_format[8 + 1];

    u64 parameters[8];
    u32 string_lengths[8];

    // Return
    s64 retval;
    s32 gdb_errno;
    bool ctrl_c;
};

/**
 * Set the current HIO request to the given address. This is how the debugged
 * app indicates to the gdbstub that it wishes to perform a request.
 *
 *  @param address The memory address of the \ref PackedGdbHioRequest.
 */
void SetHioRequest(const VAddr address);

/**
 * If there is a pending HIO request, send it to the client.
 *
 * @returns whethere any request was sent to the client.
 */
bool HandlePendingHioRequestPacket();

/**
 * Process an HIO reply from the client.
 */
void HandleHioReply(const u8* const command_buffer, const u32 command_length);

} // namespace GDBStub
