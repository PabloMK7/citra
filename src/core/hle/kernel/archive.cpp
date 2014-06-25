// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common_types.h"

#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/archive.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Kernel namespace

namespace Kernel {

class Archive : public Object {
public:
    const char* GetTypeName() const { return "Archive"; }
    const char* GetName() const { return name.c_str(); }

    static Kernel::HandleType GetStaticHandleType() { return HandleType::Archive; }
    Kernel::HandleType GetHandleType() const { return HandleType::Archive; }

    std::string name; ///< Name of archive (optional)

    /**
     * Wait for kernel object to synchronize
     * @param wait Boolean wait set if current thread should wait as a result of sync operation
     * @return Result of operation, 0 on success, otherwise error code
     */
    Result WaitSynchronization(bool* wait) {
        // TODO(bunnei): ImplementMe
        ERROR_LOG(OSHLE, "unimplemented function");
        return 0;
    }
};

/**
 * Creates an Archive
 * @param name Optional name of Archive
 * @param handle Handle to newly created archive object
 * @return Newly created Archive object
 */
Archive* CreateArchive(Handle& handle, const std::string& name) {
    Archive* archive = new Archive;
    handle = Kernel::g_object_pool.Create(archive);
    archive->name = name;
    return archive;
}

/**
 * Creates an Archive
 * @param name Optional name of Archive
 * @return Handle to newly created Archive object
 */
Handle CreateArchive(const std::string& name) {
    Handle handle;
    Archive* archive = CreateArchive(handle, name);
    return handle;
}

} // namespace Kernel
