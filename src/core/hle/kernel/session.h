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

enum DescriptorType : u32 {
    // Buffer related desciptors types (mask : 0x0F)
    StaticBuffer = 0x02,
    PXIBuffer    = 0x04,
    MappedBuffer = 0x08,
    // Handle related descriptors types (mask : 0x30, but need to check for buffer related descriptors first )
    CopyHandle   = 0x00,
    MoveHandle   = 0x10,
    CallingPid   = 0x20,
};

/**
 * @brief Creates a command header to be used for IPC
 * @param command_id            ID of the command to create a header for.
 * @param normal_params         Size of the normal parameters in words. Up to 63.
 * @param translate_params_size Size of the translate parameters in words. Up to 63.
 * @return The created IPC header.
 *
 * Normal parameters are sent directly to the process while the translate parameters might go through modifications and checks by the kernel.
 * The translate parameters are described by headers generated with the IPC::*Desc functions.
 *
 * @note While #normal_params is equivalent to the number of normal parameters, #translate_params_size includes the size occupied by the translate parameters headers.
 */
constexpr u32 MakeHeader(u16 command_id, unsigned int normal_params, unsigned int translate_params_size) {
    return (u32(command_id) << 16) | ((u32(normal_params) & 0x3F) << 6) | (u32(translate_params_size) & 0x3F);
}

union Header {
    u32 raw;
    BitField< 0, 6, u32> translate_params_size;
    BitField< 6, 6, u32> normal_params;
    BitField<16, 16, u32> command_id;
};

inline Header ParseHeader(u32 header) {
    return{ header };
}

constexpr u32 MoveHandleDesc(u32 num_handles = 1) {
    return MoveHandle | ((num_handles - 1) << 26);
}

constexpr u32 CopyHandleDesc(u32 num_handles = 1) {
    return CopyHandle | ((num_handles - 1) << 26);
}

constexpr u32 CallingPidDesc() {
    return CallingPid;
}

constexpr bool isHandleDescriptor(u32 descriptor) {
    return (descriptor & 0xF) == 0x0;
}

constexpr u32 HandleNumberFromDesc(u32 handle_descriptor) {
    return (handle_descriptor >> 26) + 1;
}

constexpr u32 StaticBufferDesc(u32 size, u8 buffer_id) {
    return StaticBuffer | (size << 14) | ((buffer_id & 0xF) << 10);
}

union StaticBufferDescInfo {
    u32 raw;
    BitField< 10, 4, u32> buffer_id;
    BitField< 14, 18, u32> size;
};

inline StaticBufferDescInfo ParseStaticBufferDesc(const u32 desc) {
    return{ desc };
}

/**
 * @brief Creates a header describing a buffer to be sent over PXI.
 * @param size         Size of the buffer. Max 0x00FFFFFF.
 * @param buffer_id    The Id of the buffer. Max 0xF.
 * @param is_read_only true if the buffer is read-only. If false, the buffer is considered to have read-write access.
 * @return The created PXI buffer header.
 *
 * The next value is a phys-address of a table located in the BASE memregion.
 */
inline u32 PXIBufferDesc(u32 size, unsigned buffer_id, bool is_read_only) {
    u32 type = PXIBuffer;
    if (is_read_only) type |= 0x2;
    return  type | (size << 8) | ((buffer_id & 0xF) << 4);
}

enum MappedBufferPermissions {
    R = 1,
    W = 2,
    RW = R | W,
};

constexpr u32 MappedBufferDesc(u32 size, MappedBufferPermissions perms) {
    return MappedBuffer | (size << 4) | (u32(perms) << 1);
}

union MappedBufferDescInfo {
    u32 raw;
    BitField< 4, 28, u32> size;
    BitField< 1, 2, MappedBufferPermissions> perms;
};

inline MappedBufferDescInfo ParseMappedBufferDesc(const u32 desc) {
    return{ desc };
}

inline DescriptorType GetDescriptorType(u32 descriptor) {
    // Note: Those checks must be done in this order
    if (isHandleDescriptor(descriptor))
        return (DescriptorType)(descriptor & 0x30);

    // handle the fact that the following descriptors can have rights
    if (descriptor & MappedBuffer)
        return MappedBuffer;

    if (descriptor & PXIBuffer)
        return PXIBuffer;

    return StaticBuffer;
}

} // namespace IPC

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
inline u32* GetCommandBuffer(const int offset = 0) {
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
