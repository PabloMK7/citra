// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"

#include "core/hle/service/service.h"
#include "core/hle/service/nim/nim.h"
#include "core/hle/service/nim/nim_aoc.h"
#include "core/hle/service/nim/nim_s.h"
#include "core/hle/service/nim/nim_u.h"

#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/hle.h"

namespace Service {
namespace NIM {

void CheckSysUpdateAvailable(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // No update available

    LOG_WARNING(Service_NWM, "(STUBBED) called");
}

void Init() {
    using namespace Kernel;

    AddService(new NIM_AOC_Interface);
    AddService(new NIM_S_Interface);
    AddService(new NIM_U_Interface);
}

void Shutdown() {
}

} // namespace NIM

} // namespace Service
