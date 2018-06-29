// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/mvd/mvd.h"
#include "core/hle/service/mvd/mvd_std.h"

namespace Service {
namespace MVD {

void InstallInterfaces(SM::ServiceManager& service_manager) {
    std::make_shared<MVD_STD>()->InstallAsService(service_manager);
}

} // namespace MVD
} // namespace Service
