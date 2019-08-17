// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cinttypes>
#include "core/core.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/apt/ns.h"
#include "core/loader/loader.h"

namespace Service::NS {

std::shared_ptr<Kernel::Process> LaunchTitle(FS::MediaType media_type, u64 title_id) {
    std::string path = AM::GetTitleContentPath(media_type, title_id);
    auto loader = Loader::GetLoader(path);

    if (!loader) {
        LOG_WARNING(Service_NS, "Could not find .app for title 0x{:016x}", title_id);
        return nullptr;
    }

    std::shared_ptr<Kernel::Process> process;
    Loader::ResultStatus result = loader->Load(process);

    if (result != Loader::ResultStatus::Success) {
        LOG_WARNING(Service_NS, "Error loading .app for title 0x{:016x}", title_id);
        return nullptr;
    }

    return process;
}

} // namespace Service::NS
