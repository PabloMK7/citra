// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/gdbstub/gdbstub.h"
#include "core/gdbstub/hio.h"

namespace GDBStub {

namespace {

static VAddr current_hio_request_addr;
static PackedGdbHioRequest current_hio_request;
static std::atomic<bool> sent_request{false};

} // namespace

void SetHioRequest(const VAddr addr) {
    if (!IsServerEnabled()) {
        LOG_WARNING(Debug_GDBStub, "HIO requested but GDB stub is not running");
        return;
    }

    if (current_hio_request_addr != 0) {
        LOG_WARNING(Debug_GDBStub, "HIO requested while already in progress!");
        return;
    }

    const auto process = Core::System::GetInstance().Kernel().GetCurrentProcess();
    if (!Memory::IsValidVirtualAddress(*process, addr)) {
        LOG_WARNING(Debug_GDBStub, "Invalid address for HIO request");
        return;
    }

    auto& memory = Core::System::GetInstance().Memory();
    memory.ReadBlock(*process, addr, &current_hio_request, sizeof(PackedGdbHioRequest));

    if (std::string_view{current_hio_request.magic} != "GDB") {
        LOG_WARNING(Debug_GDBStub, "Invalid HIO request sent by application");
        current_hio_request_addr = 0;
        current_hio_request = {};
    } else {
        current_hio_request_addr = addr;
        sent_request = false;
        LOG_DEBUG(Debug_GDBStub, "HIO request initiated");
    }
}

bool HandleHioReply(const u8* const command_buffer, const u32 command_length) {
    if (current_hio_request_addr == 0 || !sent_request) {
        LOG_WARNING(Debug_GDBStub, "Got HIO reply but never sent a request");
        // TODO send error reply packet?
        return false;
    }

    auto* command_pos = command_buffer + 1;

    if (*command_pos == 0 || *command_pos == ',') {
        // return GDB_ReplyErrno(ctx, EILSEQ);
        return false;
    }

    // Set the sign of the retval
    if (*command_pos == '-') {
        command_pos++;
        current_hio_request.retval = -1;
    } else if (*command_pos == '+') {
        command_pos++;
        current_hio_request.retval = 1;
    } else {
        current_hio_request.retval = 1;
    }

    const auto retval_end = std::find(command_pos, command_buffer + command_length, ',');
    if (retval_end >= command_buffer + command_length) {
        // return GDB_ReplyErrno(ctx, EILSEQ);
        return false;
    }
    u64 retval = (u64)HexToInt(command_pos, retval_end - command_pos);
    command_pos = retval_end + 1;

    current_hio_request.retval *= retval;
    current_hio_request.gdb_errno = 0;
    current_hio_request.ctrl_c = 0;

    if (*command_pos != 0) {
        u32 errno_;
        // GDB protocol technically allows errno to have a +/- prefix but this will never happen.
        const auto errno_end = std::find(command_pos, command_buffer + command_length, ',');
        errno_ = HexToInt(command_pos, errno_end - command_pos);
        command_pos = errno_end + 1;

        current_hio_request.gdb_errno = (int)errno_;

        if (*command_pos != 0) {
            if (*command_pos != 'C') {
                return false;
                // return GDB_ReplyErrno(ctx, EILSEQ);
            }

            current_hio_request.ctrl_c = true;
        }
    }

    std::fill(std::begin(current_hio_request.param_format),
              std::end(current_hio_request.param_format), 0);

    LOG_DEBUG(Debug_GDBStub, "HIO reply: {{retval = {}, errno = {}, ctrl_c = {}}}",
              current_hio_request.retval, current_hio_request.gdb_errno,
              current_hio_request.ctrl_c);

    const auto process = Core::System::GetInstance().Kernel().GetCurrentProcess();
    // should have been checked when we first initialized the request,
    // but just double check again before we write to memory
    if (!Memory::IsValidVirtualAddress(*process, current_hio_request_addr)) {
        LOG_WARNING(Debug_GDBStub, "Invalid address {:X} to write HIO request",
                    current_hio_request_addr);
        return false;
    }

    auto& memory = Core::System::GetInstance().Memory();
    memory.WriteBlock(*process, current_hio_request_addr, &current_hio_request,
                      sizeof(PackedGdbHioRequest));

    current_hio_request = PackedGdbHioRequest{};
    current_hio_request_addr = 0;
    sent_request = false;

    return true;
}

bool HasHioRequest() {
    return current_hio_request_addr != 0 && !sent_request;
}

std::string BuildHioRequestPacket() {
    std::stringstream packet;
    // TODO:use the IntToGdbHex funcs instead std::hex ?
    packet << "F" << current_hio_request.function_name << std::hex;

    u32 nStr = 0;

    for (u32 i = 0; i < 8 && current_hio_request.param_format[i] != 0; i++) {
        switch (current_hio_request.param_format[i]) {
        case 'i':
        case 'I':
        case 'p':
            packet << "," << (u32)current_hio_request.parameters[i];
            break;
        case 'l':
        case 'L':
            packet << "," << current_hio_request.parameters[i];
            break;
        case 's':
            packet << "," << (u32)current_hio_request.parameters[i] << "/"
                   << current_hio_request.string_lengths[nStr++];
            break;
        default:
            packet << '\0';
            break;
        }
    }

    LOG_DEBUG(Debug_GDBStub, "HIO request packet: {}", packet.str());
    sent_request = true;

    return packet.str();
}

} // namespace GDBStub
