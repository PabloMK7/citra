// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/qtm/qtm_s.h"

SERIALIZE_EXPORT_IMPL(Service::QTM::QTM_S)

namespace Service::QTM {

QTM_S::QTM_S() : ServiceFramework("qtm:s", 2) {
    static const FunctionInfo functions[] = {
        // qtm common commands
        // clang-format off
        {IPC::MakeHeader(0x0001, 2, 0), nullptr, "GetHeadtrackingInfoRaw"},
        {IPC::MakeHeader(0x0002, 2, 0), nullptr, "GetHeadtrackingInfo"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::QTM
