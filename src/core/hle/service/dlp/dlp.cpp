// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/dlp/dlp.h"
#include "core/hle/service/dlp/dlp_clnt.h"
#include "core/hle/service/dlp/dlp_fkcl.h"
#include "core/hle/service/dlp/dlp_srvr.h"

namespace Service {
namespace DLP {

void InstallInterfaces(SM::ServiceManager& service_manager) {
    std::make_shared<DLP_CLNT>()->InstallAsService(service_manager);
    std::make_shared<DLP_FKCL>()->InstallAsService(service_manager);
    std::make_shared<DLP_SRVR>()->InstallAsService(service_manager);
}

} // namespace DLP
} // namespace Service
