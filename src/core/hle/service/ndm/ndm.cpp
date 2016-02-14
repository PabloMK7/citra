// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/hle/service/service.h"
#include "core/hle/service/ndm/ndm.h"
#include "core/hle/service/ndm/ndm_u.h"

namespace Service {
namespace NDM {

void SuspendDaemons(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    LOG_WARNING(Service_NDM, "(STUBBED) bit_mask=0x%08X ", cmd_buff[1]);

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
}

void ResumeDaemons(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    LOG_WARNING(Service_NDM, "(STUBBED) bit_mask=0x%08X ", cmd_buff[1]);

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
}

void OverrideDefaultDaemons(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    LOG_WARNING(Service_NDM, "(STUBBED) bit_mask=0x%08X ", cmd_buff[1]);

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
}

void Init() {
    AddService(new NDM_U_Interface);
}

void Shutdown() {

}

}// namespace NDM
}// namespace Service
