// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <fmt/ranges.h>
#include "common/string_util.h"
#include "core/core.h"
#include "core/gdbstub/gdbstub.h"
#include "core/gdbstub/hio.h"

namespace GDBStub {

namespace {

static VAddr current_hio_request_addr;
static PackedGdbHioRequest current_hio_request;

enum class Status {
    NoRequest,
    NotSent,
    SentWaitingReply,
};

static std::atomic<Status> request_status{Status::NoRequest};

} // namespace

void SetHioRequest(const VAddr addr) {
    if (!IsServerEnabled()) {
        LOG_WARNING(Debug_GDBStub, "HIO requested but GDB stub is not running");
        return;
    }

    if (HasPendingHioRequest() || WaitingForHioReply()) {
        LOG_WARNING(Debug_GDBStub, "HIO requested while already in progress!");
        return;
    }

    auto& memory = Core::System::GetInstance().Memory();
    const auto process = Core::System::GetInstance().Kernel().GetCurrentProcess();

    if (!memory.IsValidVirtualAddress(*process, addr)) {
        LOG_WARNING(Debug_GDBStub, "Invalid address for HIO request");
        return;
    }

    memory.ReadBlock(*process, addr, &current_hio_request, sizeof(PackedGdbHioRequest));

    std::string_view magic{current_hio_request.magic};
    if (magic != "GDB") {
        LOG_WARNING(Debug_GDBStub, "Invalid HIO request sent by application: magic '{}'", magic);
        current_hio_request_addr = 0;
        current_hio_request = {};
        request_status = Status::NoRequest;
        return;
    }

    LOG_DEBUG(Debug_GDBStub, "HIO request initiated at 0x{:X}", addr);
    current_hio_request_addr = addr;
    request_status = Status::NotSent;

    // Now halt, so that no further instructions are executed until the request
    // is processed by the client. We will continue after the reply comes back
    Break();
    SetCpuHaltFlag(true);
    SetCpuStepFlag(false);
    Core::GetRunningCore().ClearInstructionCache();
}

bool HandleHioReply(const u8* const command_buffer, const u32 command_length) {
    if (!WaitingForHioReply()) {
        LOG_WARNING(Debug_GDBStub, "Got HIO reply but never sent a request");
        // TODO send error reply packet?
        return false;
    }

    // Skip 'F' header
    auto* command_pos = command_buffer + 1;

    if (*command_pos == 0 || *command_pos == ',') {
        // return GDB_ReplyErrno(ctx, EILSEQ);
        return false;
    }

    // Set the sign of the retval
    if (*command_pos == '-') {
        command_pos++;
        current_hio_request.retval = -1;
    } else {
        if (*command_pos == '+') {
            command_pos++;
        }

        current_hio_request.retval = 1;
    }

    const std::string command_str{command_pos, command_buffer + command_length};
    std::vector<std::string> command_parts;
    Common::SplitString(command_str, ',', command_parts);

    if (command_parts.empty() || command_parts.size() > 3) {
        LOG_WARNING(Debug_GDBStub, "unexpected reply packet size: {}", command_parts);
        // return GDB_ReplyErrno(ctx, EILSEQ);
        return false;
    }

    u64 unsigned_retval = HexToInt((u8*)command_parts[0].data(), command_parts[0].size());
    current_hio_request.retval *= unsigned_retval;

    if (command_parts.size() > 1) {
        // Technically the errno could be signed but in practice this doesn't happen
        current_hio_request.gdb_errno =
            HexToInt((u8*)command_parts[1].data(), command_parts[1].size());
    } else {
        current_hio_request.gdb_errno = 0;
    }

    current_hio_request.ctrl_c = false;

    if (command_parts.size() > 2 && !command_parts[2].empty()) {
        if (command_parts[2][0] != 'C') {
            return false;
            // return GDB_ReplyErrno(ctx, EILSEQ);
        }

        // for now we just ignore any trailing ";..." attachments
        current_hio_request.ctrl_c = true;
    }

    std::fill(std::begin(current_hio_request.param_format),
              std::end(current_hio_request.param_format), 0);

    LOG_TRACE(Debug_GDBStub, "HIO reply: {{retval = {}, errno = {}, ctrl_c = {}}}",
              current_hio_request.retval, current_hio_request.gdb_errno,
              current_hio_request.ctrl_c);

    const auto process = Core::System::GetInstance().Kernel().GetCurrentProcess();
    auto& memory = Core::System::GetInstance().Memory();

    // should have been checked when we first initialized the request,
    // but just double check again before we write to memory
    if (!memory.IsValidVirtualAddress(*process, current_hio_request_addr)) {
        LOG_WARNING(Debug_GDBStub, "Invalid address {:#X} to write HIO reply",
                    current_hio_request_addr);
        return false;
    }

    memory.WriteBlock(*process, current_hio_request_addr, &current_hio_request,
                      sizeof(PackedGdbHioRequest));

    current_hio_request = {};
    current_hio_request_addr = 0;
    request_status = Status::NoRequest;

    return true;
}

bool HasPendingHioRequest() {
    return current_hio_request_addr != 0 && request_status == Status::NotSent;
}

bool WaitingForHioReply() {
    return current_hio_request_addr != 0 && request_status == Status::SentWaitingReply;
}

std::string BuildHioRequestPacket() {
    std::string packet = fmt::format("F{}", current_hio_request.function_name);
    // TODO: should we use the IntToGdbHex funcs instead of fmt::format_to ?

    u32 nStr = 0;

    for (u32 i = 0; i < 8 && current_hio_request.param_format[i] != 0; i++) {
        u64 param = current_hio_request.parameters[i];

        switch (current_hio_request.param_format[i]) {
        case 'i':
        case 'I':
        case 'p':
            // For pointers and 32-bit ints, truncate before sending
            param = static_cast<u32>(param);

        case 'l':
        case 'L':
            fmt::format_to(std::back_inserter(packet), ",{:x}", param);
            break;

        case 's':
            // strings are written as {pointer}/{length}
            fmt::format_to(std::back_inserter(packet), ",{:x}/{:x}",
                           (u32)current_hio_request.parameters[i],
                           current_hio_request.string_lengths[nStr++]);
            break;

        default:
            // TODO: early return? Or break out of this loop?
            break;
        }
    }

    LOG_TRACE(Debug_GDBStub, "HIO request packet: {}", packet);

    // technically we should set this _after_ the reply is sent...
    request_status = Status::SentWaitingReply;

    return packet;
}

} // namespace GDBStub
