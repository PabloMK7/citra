// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace PM {

/// Initializes the PM services.
void InstallInterfaces(SM::ServiceManager& service_manager);

} // namespace PM
} // namespace Service
