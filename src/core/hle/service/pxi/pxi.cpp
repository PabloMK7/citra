// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/pxi/dev.h"
#include "core/hle/service/pxi/pxi.h"

namespace Service {
namespace PXI {

void InstallInterfaces(SM::ServiceManager& service_manager) {
    std::make_shared<DEV>()->InstallAsService(service_manager);
}

} // namespace PXI
} // namespace Service
