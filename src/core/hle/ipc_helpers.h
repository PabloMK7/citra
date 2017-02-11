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

    /**
     * @brief Retrieves the address of a static buffer, used when a buffer is needed for output
     * @param buffer_id The index of the static buffer
     * @param data_size If non-null, will store the size of the buffer
     */
    VAddr PeekStaticBuffer(u8 buffer_id, size_t* data_size = nullptr) const {
        u32* static_buffer = cmdbuf + Kernel::kStaticBuffersOffset / 4 + buffer_id * 2;
        if (data_size)
            *data_size = StaticBufferDescInfo{static_buffer[0]}.size;
        return static_buffer[1];
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

    template <typename First, typename... Other>
    void Push(const First& first_value, const Other&... other_values);

    /**
     * @brief Copies the content of the given trivially copyable class to the buffer as a normal
     * param
     * @note: The input class must be correctly packed/padded to fit hardware layout.
     */
    template <typename T>
    void PushRaw(const T& value);

    // TODO : ensure that translate params are added after all regular params
    template <typename... H>
    void PushCopyHandles(H... handles);

    template <typename... H>
    void PushMoveHandles(H... handles);

    void PushCurrentPIDHandle();

    void PushStaticBuffer(VAddr buffer_vaddr, u32 size, u8 buffer_id);

    void PushMappedBuffer(VAddr buffer_vaddr, u32 size, MappedBufferPermissions perms);
};

/// Push ///

template <>
inline void RequestBuilder::Push(u32 value) {
    cmdbuf[index++] = value;
}

template <typename T>
void RequestBuilder::PushRaw(const T& value) {
    static_assert(std::is_trivially_copyable<T>(), "Raw types should be trivially copyable");
    std::memcpy(cmdbuf + index, &value, sizeof(T));
    index += (sizeof(T) + 3) / 4; // round up to word length
}

template <>
inline void RequestBuilder::Push(u8 value) {
    PushRaw(value);
}

template <>
inline void RequestBuilder::Push(u16 value) {
    PushRaw(value);
}

template <>
inline void RequestBuilder::Push(u64 value) {
    Push(static_cast<u32>(value));
    Push(static_cast<u32>(value >> 32));
}

template <>
inline void RequestBuilder::Push(bool value) {
    Push(static_cast<u8>(value));
}

template <>
inline void RequestBuilder::Push(ResultCode value) {
    Push(value.raw);
}

template <typename First, typename... Other>
void RequestBuilder::Push(const First& first_value, const Other&... other_values) {
    Push(first_value);
    Push(other_values...);
}

template <typename... H>
inline void RequestBuilder::PushCopyHandles(H... handles) {
    Push(CopyHandleDesc(sizeof...(H)));
    Push(static_cast<Kernel::Handle>(handles)...);
}

template <typename... H>
inline void RequestBuilder::PushMoveHandles(H... handles) {
    Push(MoveHandleDesc(sizeof...(H)));
    Push(static_cast<Kernel::Handle>(handles)...);
}

inline void RequestBuilder::PushCurrentPIDHandle() {
    Push(CallingPidDesc());
    Push(u32(0));
}

inline void RequestBuilder::PushStaticBuffer(VAddr buffer_vaddr, u32 size, u8 buffer_id) {
    Push(StaticBufferDesc(size, buffer_id));
    Push(buffer_vaddr);
}

inline void RequestBuilder::PushMappedBuffer(VAddr buffer_vaddr, u32 size,
                                             MappedBufferPermissions perms) {
    Push(MappedBufferDesc(size, perms));
    Push(buffer_vaddr);
}

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

    template <typename T>
    void Pop(T& value);

    template <typename First, typename... Other>
    void Pop(First& first_value, Other&... other_values);

    Kernel::Handle PopHandle();

    template <typename... H>
    void PopHandles(H&... handles);

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
    VAddr PopStaticBuffer(size_t* data_size = nullptr, bool useStaticBuffersToGetVaddr = false);

    /**
     * @brief Pops the mapped buffer vaddr
     * @return                  The virtual address of the buffer
     * @param[out] data_size    If non-null, the pointed value will be set to the size of the data
     * given by the source process
     * @param[out] buffer_perms If non-null, the pointed value will be set to the permissions of the
     * buffer
     */
    VAddr PopMappedBuffer(size_t* data_size = nullptr,
                          MappedBufferPermissions* buffer_perms = nullptr);

    /**
     * @brief Reads the next normal parameters as a struct, by copying it
     * @note: The output class must be correctly packed/padded to fit hardware layout.
     */
    template <typename T>
    void PopRaw(T& value);

    /**
     * @brief Reads the next normal parameters as a struct, by copying it into a new value
     * @note: The output class must be correctly packed/padded to fit hardware layout.
     */
    template <typename T>
    T PopRaw();
};

/// Pop ///

template <>
inline u32 RequestParser::Pop() {
    return cmdbuf[index++];
}

template <typename T>
void RequestParser::PopRaw(T& value) {
    static_assert(std::is_trivially_copyable<T>(), "Raw types should be trivially copyable");
    std::memcpy(&value, cmdbuf + index, sizeof(T));
    index += (sizeof(T) + 3) / 4; // round up to word length
}

template <typename T>
T RequestParser::PopRaw() {
    T value;
    PopRaw(value);
    return value;
}

template <>
inline u8 RequestParser::Pop() {
    return PopRaw<u8>();
}

template <>
inline u16 RequestParser::Pop() {
    return PopRaw<u16>();
}

template <>
inline u64 RequestParser::Pop() {
    const u64 lsw = Pop<u32>();
    const u64 msw = Pop<u32>();
    return msw << 32 | lsw;
}

template <>
inline bool RequestParser::Pop() {
    return Pop<u8>() != 0;
}

template <>
inline ResultCode RequestParser::Pop() {
    return ResultCode{Pop<u32>()};
}

template <typename T>
void RequestParser::Pop(T& value) {
    value = Pop<T>();
}

template <typename First, typename... Other>
void RequestParser::Pop(First& first_value, Other&... other_values) {
    first_value = Pop<First>();
    Pop(other_values...);
}

inline Kernel::Handle RequestParser::PopHandle() {
    const u32 handle_descriptor = Pop<u32>();
    DEBUG_ASSERT_MSG(IsHandleDescriptor(handle_descriptor),
                     "Tried to pop handle(s) but the descriptor is not a handle descriptor");
    DEBUG_ASSERT_MSG(HandleNumberFromDesc(handle_descriptor) == 1,
                     "Descriptor indicates that there isn't exactly one handle");
    return Pop<Kernel::Handle>();
}

template <typename... H>
void RequestParser::PopHandles(H&... handles) {
    const u32 handle_descriptor = Pop<u32>();
    const int handles_number = sizeof...(H);
    DEBUG_ASSERT_MSG(IsHandleDescriptor(handle_descriptor),
                     "Tried to pop handle(s) but the descriptor is not a handle descriptor");
    DEBUG_ASSERT_MSG(handles_number == HandleNumberFromDesc(handle_descriptor),
                     "Number of handles doesn't match the descriptor");
    Pop(static_cast<Kernel::Handle&>(handles)...);
}

inline VAddr RequestParser::PopStaticBuffer(size_t* data_size, bool useStaticBuffersToGetVaddr) {
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

inline VAddr RequestParser::PopMappedBuffer(size_t* data_size,
                                            MappedBufferPermissions* buffer_perms) {
    const u32 sbuffer_descriptor = Pop<u32>();
    MappedBufferDescInfo bufferInfo{sbuffer_descriptor};
    if (data_size != nullptr)
        *data_size = bufferInfo.size;
    if (buffer_perms != nullptr)
        *buffer_perms = bufferInfo.perms;
    return Pop<VAddr>();
}

} // namespace IPC
