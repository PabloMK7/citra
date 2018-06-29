// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/qtm/qtm.h"
#include "core/hle/service/qtm/qtm_c.h"
#include "core/hle/service/qtm/qtm_s.h"
#include "core/hle/service/qtm/qtm_sp.h"
#include "core/hle/service/qtm/qtm_u.h"

namespace Service {
namespace QTM {

void InstallInterfaces(SM::ServiceManager& service_manager) {
    std::make_shared<QTM_C>()->InstallAsService(service_manager);
    std::make_shared<QTM_S>()->InstallAsService(service_manager);
    std::make_shared<QTM_SP>()->InstallAsService(service_manager);
    std::make_shared<QTM_U>()->InstallAsService(service_manager);
}

} // namespace QTM
} // namespace Service
