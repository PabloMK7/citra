// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/kernel/process.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/service.h"

namespace Service {
namespace NS {

/// Loads and launches the title identified by title_id in the specified media type.
Kernel::SharedPtr<Kernel::Process> LaunchTitle(FS::MediaType media_type, u64 title_id);

/// Registers all NS services with the specified service manager.
void InstallInterfaces(SM::ServiceManager& service_manager);

} // namespace NS
} // namespace Service
