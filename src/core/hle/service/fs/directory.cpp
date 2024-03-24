// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/file_sys/directory_backend.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/fs/directory.h"

namespace Service::FS {

Directory::Directory(std::unique_ptr<FileSys::DirectoryBackend>&& backend,
                     const FileSys::Path& path)
    : ServiceFramework("", 1) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0801, &Directory::Read, "Read"},
        {0x0802, &Directory::Close, "Close"},
        // clang-format on
    };
    RegisterHandlers(functions);
    this->backend = std::move(backend);
    this->path = path;
}

Directory::~Directory() {}

void Directory::Read(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 count = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();
    std::vector<FileSys::Entry> entries(count);
    LOG_TRACE(Service_FS, "Read {}: count={}", GetName(), count);
    // Number of entries actually read
    u32 read = backend->Read(static_cast<u32>(entries.size()), entries.data());
    buffer.Write(entries.data(), 0, read * sizeof(FileSys::Entry));

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(ResultSuccess);
    rb.Push(read);
    rb.PushMappedBuffer(buffer);
}

void Directory::Close(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    LOG_TRACE(Service_FS, "Close {}", GetName());
    backend->Close();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

} // namespace Service::FS
