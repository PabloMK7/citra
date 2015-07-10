// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "common/assert.h"
#include "common/common_types.h"

#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/result.h"
#include "core/memory.h"

namespace IPC {

inline u32 MakeHeader(u16 command_id, unsigned int regular_params, unsigned int translate_params) {
    return ((u32)command_id << 16) | (((u32)regular_params & 0x3F) << 6) | (((u32)translate_params & 0x3F) << 0);
}

inline u32 MoveHandleDesc(unsigned int num_handles = 1) {
    return 0x0 | ((num_handles - 1) << 26);
}

inline u32 CopyHandleDesc(unsigned int num_handles = 1) {
    return 0x10 | ((num_handles - 1) << 26);
}

inline u32 CallingPidDesc() {
    return 0x20;
}

inline u32 StaticBufferDesc(u32 size, unsigned int buffer_id) {
    return 0x2 | (size << 14) | ((buffer_id & 0xF) << 10);
}

enum MappedBufferPermissions {
    R = 2,
    W = 4,
    RW = R | W,
};

inline u32 MappedBufferDesc(u32 size, MappedBufferPermissions perms) {
    return 0x8 | (size << 4) | (u32)perms;
}

}

namespace Kernel {

static const int kCommandHeaderOffset = 0x80; ///< Offset into command buffer of header

/**
 * Returns a pointer to the command buffer in the current thread's TLS
 * TODO(Subv): This is not entirely correct, the command buffer should be copied from
 * the thread's TLS to an intermediate buffer in kernel memory, and then copied again to
 * the service handler process' memory.
 * @param offset Optional offset into command buffer
 * @return Pointer to command buffer
 */
inline static u32* GetCommandBuffer(const int offset = 0) {
    return (u32*)Memory::GetPointer(GetCurrentThread()->GetTLSAddress() + kCommandHeaderOffset + offset);
}

/**
 * Kernel object representing the client endpoint of an IPC session. Sessions are the basic CTR-OS
 * primitive for communication between different processes, and are used to implement service calls
 * to the various system services.
 *
 * To make a service call, the client must write the command header and parameters to the buffer
 * located at offset 0x80 of the TLS (Thread-Local Storage) area, then execute a SendSyncRequest
 * SVC call with its Session handle. The kernel will read the command header, using it to marshall
 * the parameters to the process at the server endpoint of the session. After the server replies to
 * the request, the response is marshalled back to the caller's TLS buffer and control is
 * transferred back to it.
 *
 * In Citra, only the client endpoint is currently implemented and only HLE calls, where the IPC
 * request is answered by C++ code in the emulator, are supported. When SendSyncRequest is called
 * with the session handle, this class's SyncRequest method is called, which should read the TLS
 * buffer and emulate the call accordingly. Since the code can directly read the emulated memory,
 * no parameter marshalling is done.
 *
 * In the long term, this should be turned into the full-fledged IPC mechanism implemented by
 * CTR-OS so that IPC calls can be optionally handled by the real implementations of processes, as
 * opposed to HLE simulations.
 */
class Session : public WaitObject {
public:
    Session();
    ~Session() override;

    std::string GetTypeName() const override { return "Session"; }

    static const HandleType HANDLE_TYPE = HandleType::Session;
    HandleType GetHandleType() const override { return HANDLE_TYPE; }

    /**
     * Handles a synchronous call to this session using HLE emulation. Emulated <-> emulated calls
     * aren't supported yet.
     */
    virtual ResultVal<bool> SyncRequest() = 0;

    // TODO(bunnei): These functions exist to satisfy a hardware test with a Session object
    // passed into WaitSynchronization. Figure out the meaning of them.

    bool ShouldWait() override {
        return true;
    }

    void Acquire() override {
        ASSERT_MSG(!ShouldWait(), "object unavailable!");
    }
};

}
