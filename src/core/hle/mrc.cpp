#include "mrc.h"
#include "hle.h"

namespace HLE {

/// Returns the coprocessor (in this case, syscore) command buffer pointer
Addr CallGetThreadCommandBuffer() {
    // Called on insruction: mrc p15, 0, r0, c13, c0, 3
    // Returns an address in OSHLE memory for the CPU to read/write to
    RETURN(CMD_BUFFER_ADDR);
    return CMD_BUFFER_ADDR;
}

/// Call an MRC operation in HLE
u32 CallMRC(ARM11_MRC_OPERATION operation) {
    switch (operation) {

    case DATA_SYNCHRONIZATION_BARRIER:
        ERROR_LOG(OSHLE, "Unimplemented MRC operation DATA_SYNCHRONIZATION_BARRIER");
        break;

    case CALL_GET_THREAD_COMMAND_BUFFER:
        return CallGetThreadCommandBuffer();

    default:
        ERROR_LOG(OSHLE, "Unimplemented MRC operation 0x%02X", operation);
        break;
    }
    return -1;
}

} // namespace
