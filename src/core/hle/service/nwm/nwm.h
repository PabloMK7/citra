// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace NWM {

/// Initialize all NWM services
void Init();
void InstallInterfaces(SM::ServiceManager& service_manager);

} // namespace NWM
} // namespace Service
