// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/ns/ns.h"
#include "core/hle/service/ns/ns_s.h"

namespace Service {
namespace NS {

void InstallInterfaces(SM::ServiceManager& service_manager) {
    std::make_shared<NS_S>()->InstallAsService(service_manager);
}

} // namespace NS
} // namespace Service
