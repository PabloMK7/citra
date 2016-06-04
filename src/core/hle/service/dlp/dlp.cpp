// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/service.h"
#include "core/hle/service/dlp/dlp.h"
#include "core/hle/service/dlp/dlp_clnt.h"
#include "core/hle/service/dlp/dlp_fkcl.h"
#include "core/hle/service/dlp/dlp_srvr.h"

namespace Service {
namespace DLP {

void Init() {
    AddService(new DLP_CLNT_Interface);
    AddService(new DLP_FKCL_Interface);
    AddService(new DLP_SRVR_Interface);
}

void Shutdown() {
}

} // namespace DLP
} // namespace Service
