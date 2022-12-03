#include "core/core.h"
#include "core/gdbstub/gdbstub.h"
#include "core/gdbstub/hio.h"

namespace GDBStub {

namespace {

static VAddr current_hio_request_addr;
static PackedGdbHioRequest current_hio_request;

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

    auto& memory = Core::System::GetInstance().Memory();
    if (!memory.IsValidVirtualAddress(*Core::System::GetInstance().Kernel().GetCurrentProcess(),
                                      addr)) {
        LOG_WARNING(Debug_GDBStub, "Invalid address for HIO request");
        return;
    }

    memory.ReadBlock(addr, &current_hio_request, sizeof(PackedGdbHioRequest));
    current_hio_request_addr = addr;

    LOG_DEBUG(Debug_GDBStub, "HIO request initiated");
}

bool HandleHioRequest(const u8* const command_buffer, const u32 command_length) {
    if (!HasHioRequest()) {
        // TODO send error reply packet?
        return false;
    }

    u64 retval{0};

    auto* command_pos = command_buffer;
    ++command_pos;

    // TODO: not totally sure what's going on here...
    if (*command_pos == 0 || *command_pos == ',') {
        // return GDB_ReplyErrno(ctx, EILSEQ);
        return false;
    } else if (*command_pos == '-') {
        command_pos++;
        current_hio_request.retval = -1;
    } else if (*command_pos == '+') {
        command_pos++;
        current_hio_request.retval = 1;
    } else {
        current_hio_request.retval = 1;
    }

    // TODO:
    // pos = GDB_ParseHexIntegerList64(&retval, pos, 1, ',');

    if (command_pos == nullptr) {
        // return GDB_ReplyErrno(ctx, EILSEQ);
        return false;
    }

    current_hio_request.retval *= retval;
    current_hio_request.gdb_errno = 0;
    current_hio_request.ctrl_c = 0;

    if (*command_pos != 0) {
        u32 errno_;
        // GDB protocol technically allows errno to have a +/- prefix but this will never happen.
        // TODO:
        // pos = GDB_ParseHexIntegerList(&errno_, ++pos, 1, ',');
        current_hio_request.gdb_errno = (int)errno_;
        if (command_pos == nullptr) {
            return false;
            // return GDB_ReplyErrno(ctx, EILSEQ);
        }

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

    auto& memory = Core::System::GetInstance().Memory();
    // should have been checked when we first initialized the request:
    assert(memory.IsValidVirtualAddress(*Core::System::GetInstance().Kernel().GetCurrentProcess(),
                                        current_hio_request_addr));

    memory.WriteBlock(current_hio_request_addr, &current_hio_request, sizeof(PackedGdbHioRequest));

    current_hio_request = PackedGdbHioRequest{};
    current_hio_request_addr = 0;

    return true;
}

bool HasHioRequest() {
    return current_hio_request_addr != 0;
}

std::string BuildHioReply() {
    char buf[256 + 1];
    char tmp[32 + 1];
    u32 nStr = 0;

    // TODO: c++ify this and use the IntToGdbHex funcs instead of snprintf

    snprintf(buf, 256, "F%s", current_hio_request.function_name);

    for (u32 i = 0; i < 8 && current_hio_request.param_format[i] != 0; i++) {
        switch (current_hio_request.param_format[i]) {
        case 'i':
        case 'I':
        case 'p':
            snprintf(tmp, 32, ",%x", (u32)current_hio_request.parameters[i]);
            break;
        case 'l':
        case 'L':
            snprintf(tmp, 32, ",%llx", current_hio_request.parameters[i]);
            break;
        case 's':
            snprintf(tmp, 32, ",%x/%zx", (u32)current_hio_request.parameters[i],
                     current_hio_request.string_lengths[nStr++]);
            break;
        default:
            tmp[0] = 0;
            break;
        }
        strcat(buf, tmp);
    }

    return std::string{buf, strlen(buf)};
}

} // namespace GDBStub
