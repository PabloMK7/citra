// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once
#include "core/hle/ipc.h"
#include "core/hle/kernel/kernel.h"

namespace IPC {

class RequestHelperBase {
protected:
    u32* cmdbuf;
    ptrdiff_t index = 1;
    Header header;

public:
    RequestHelperBase(u32* command_buffer, Header command_header)
        : cmdbuf(command_buffer), header(command_header) {}

    /// Returns the total size of the request in words
    size_t TotalSize() const {
        return 1 /* command header */ + header.normal_params_size + header.translate_params_size;
    }

    void ValidateHeader() {
        DEBUG_ASSERT_MSG(index == TotalSize(), "Operations do not match the header (cmd 0x%x)",
                         header.raw);
    }
};

class RequestBuilder : public RequestHelperBase {
public:
    RequestBuilder(u32* command_buffer, Header command_header)
        : RequestHelperBase(command_buffer, command_header) {
        cmdbuf[0] = header.raw;
    }
    explicit RequestBuilder(u32* command_buffer, u32 command_header)
        : RequestBuilder(command_buffer, Header{command_header}) {}
    RequestBuilder(u32* command_buffer, u16 command_id, unsigned normal_params_size,
                   unsigned translate_params_size)
        : RequestBuilder(command_buffer,
                         MakeHeader(command_id, normal_params_size, translate_params_size)) {}

    // Validate on destruction, as there shouldn't be any case where we don't want it
    ~RequestBuilder() {
        ValidateHeader();
    }

    template <typename T>
    void Push(T value);

    template <>
    void Push(u32);

    template <typename First, class... Other>
    void Push(First first_value, const Other&... other_values) {
        Push(first_value);
        Push(other_values...);
    }

    /**
     * @brief Copies the content of the given trivially copyable class to the buffer as a normal
     * param
     * @note: The input class must be correctly packed/padded to fit hardware layout.
     */
    template <typename T>
    void PushRaw(const T& value) {
        static_assert(std::is_trivially_copyable<T>(), "Raw types should be trivially copyable");
        std::memcpy(cmdbuf + index, &value, sizeof(T));
        index += (sizeof(T) + 3) / 4; // round up to word length
    }

    // TODO : ensure that translate params are added after all regular params
    template <typename... H>
    void PushCopyHandles(H... handles) {
        Push(CopyHandleDesc(sizeof...(H)));
        Push(static_cast<Kernel::Handle>(handles)...);
    }

    template <typename... H>
    void PushMoveHandles(H... handles) {
        Push(MoveHandleDesc(sizeof...(H)));
        Push(static_cast<Kernel::Handle>(handles)...);
    }

    void PushCurrentPIDHandle() {
        Push(CallingPidDesc());
        Push<u32>(0);
    }

    void PushStaticBuffer(VAddr buffer_vaddr, u32 size, u8 buffer_id) {
        Push(StaticBufferDesc(size, buffer_id));
        Push(buffer_vaddr);
    }

    void PushMappedBuffer(VAddr buffer_vaddr, u32 size, MappedBufferPermissions perms) {
        Push(MappedBufferDesc(size, perms));
        Push(buffer_vaddr);
    }
};

class RequestParser : public RequestHelperBase {
public:
    RequestParser(u32* command_buffer, Header command_header)
        : RequestHelperBase(command_buffer, command_header) {}
    explicit RequestParser(u32* command_buffer, u32 command_header)
        : RequestParser(command_buffer, Header{command_header}) {}
    RequestParser(u32* command_buffer, u16 command_id, unsigned normal_params_size,
                  unsigned translate_params_size)
        : RequestParser(command_buffer,
                        MakeHeader(command_id, normal_params_size, translate_params_size)) {}

    RequestBuilder MakeBuilder(u32 normal_params_size, u32 translate_params_size,
                               bool validateHeader = true) {
        if (validateHeader)
            ValidateHeader();
        Header builderHeader{
            MakeHeader(header.command_id, normal_params_size, translate_params_size)};
        return {cmdbuf, builderHeader};
    }

    template <typename T>
    T Pop();

    template <>
    u32 Pop<u32>();

    template <typename T>
    void Pop(T& value) {
        value = Pop<T>();
    }

    template <typename First, class... Other>
    void Pop(First& first_value, Other&... other_values) {
        first_value = Pop<First>();
        Pop(other_values...);
    }

    Kernel::Handle PopHandle() {
        const u32 handle_descriptor = Pop<u32>();
        DEBUG_ASSERT_MSG(IsHandleDescriptor(handle_descriptor),
                         "Tried to pop handle(s) but the descriptor is not a handle descriptor");
        DEBUG_ASSERT_MSG(HandleNumberFromDesc(handle_descriptor) == 1,
                         "Descriptor indicates that there isn't exactly one handle");
        return Pop<Kernel::Handle>();
    }

    template <typename... H>
    void PopHandles(H&... handles) {
        const u32 handle_descriptor = Pop<u32>();
        const int handles_number = sizeof...(H);
        DEBUG_ASSERT_MSG(IsHandleDescriptor(handle_descriptor),
                         "Tried to pop handle(s) but the descriptor is not a handle descriptor");
        DEBUG_ASSERT_MSG(handles_number == HandleNumberFromDesc(handle_descriptor),
                         "Number of handles doesn't match the descriptor");
        Pop(static_cast<Kernel::Handle&>(handles)...);
    }

    /**
     * @brief Pops the static buffer vaddr
     * @return                  The virtual address of the buffer
     * @param[out] data_size    If non-null, the pointed value will be set to the size of the data
     * @param[out] useStaticBuffersToGetVaddr Indicates if we should read the vaddr from the static
     * buffers (which is the correct thing to do, but no service presently implement it) instead of
     * using the same value as the process who sent the request
     * given by the source process
     *
     * Static buffers must be set up before any IPC request using those is sent.
     * It is the duty of the process (usually services) to allocate and set up the receiving static
     * buffer information
     * Please note that the setup uses virtual addresses.
     */
    VAddr PopStaticBuffer(size_t* data_size = nullptr, bool useStaticBuffersToGetVaddr = false) {
        const u32 sbuffer_descriptor = Pop<u32>();
        StaticBufferDescInfo bufferInfo{sbuffer_descriptor};
        if (data_size != nullptr)
            *data_size = bufferInfo.size;
        if (!useStaticBuffersToGetVaddr)
            return Pop<VAddr>();
        else {
            ASSERT_MSG(0, "remove the assert if multiprocess/IPC translation are implemented.");
            // The buffer has already been copied to the static buffer by the kernel during
            // translation
            Pop<VAddr>(); // Pop the calling process buffer address
                          // and get the vaddr from the static buffers
            return cmdbuf[(0x100 >> 2) + bufferInfo.buffer_id * 2 + 1];
        }
    }

    /**
     * @brief Pops the mapped buffer vaddr
     * @return                  The virtual address of the buffer
     * @param[out] data_size    If non-null, the pointed value will be set to the size of the data
     * given by the source process
     * @param[out] buffer_perms If non-null, the pointed value will be set to the permissions of the
     * buffer
     */
    VAddr PopMappedBuffer(size_t* data_size = nullptr,
                          MappedBufferPermissions* buffer_perms = nullptr) {
        const u32 sbuffer_descriptor = Pop<u32>();
        MappedBufferDescInfo bufferInfo{sbuffer_descriptor};
        if (data_size != nullptr)
            *data_size = bufferInfo.size;
        if (buffer_perms != nullptr)
            *buffer_perms = bufferInfo.perms;
        return Pop<VAddr>();
    }

    /**
     * @brief Reads the next normal parameters as a struct, by copying it
     * @note: The output class must be correctly packed/padded to fit hardware layout.
     */
    template <typename T>
    void PopRaw(T& value) {
        static_assert(std::is_trivially_copyable<T>(), "Raw types should be trivially copyable");
        std::memcpy(&value, cmdbuf + index, sizeof(T));
        index += (sizeof(T) + 3) / 4; // round up to word length
    }
};

/// Pop ///

template <>
inline u32 RequestParser::Pop<u32>() {
    return cmdbuf[index++];
}

template <>
inline u64 RequestParser::Pop<u64>() {
    const u64 lsw = Pop<u32>();
    const u64 msw = Pop<u32>();
    return msw << 32 | lsw;
}

template <>
inline ResultCode RequestParser::Pop<ResultCode>() {
    return ResultCode{Pop<u32>()};
}

/// Push ///

template <>
inline void RequestBuilder::Push<u32>(u32 value) {
    cmdbuf[index++] = value;
}

template <>
inline void RequestBuilder::Push<u64>(u64 value) {
    Push(static_cast<u32>(value));
    Push(static_cast<u32>(value >> 32));
}

template <>
inline void RequestBuilder::Push<ResultCode>(ResultCode value) {
    Push(value.raw);
}

} // namespace IPC
