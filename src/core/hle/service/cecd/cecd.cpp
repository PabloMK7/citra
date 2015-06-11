// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"

#include "core/hle/service/service.h"
#include "core/hle/service/cecd/cecd.h"
#include "core/hle/service/cecd/cecd_s.h"
#include "core/hle/service/cecd/cecd_u.h"

#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/hle.h"

namespace Service {
namespace CECD {

void Init() {
    using namespace Kernel;

    AddService(new CECD_S_Interface);
    AddService(new CECD_U_Interface);
}

void Shutdown() {
}

} // namespace CECD

} // namespace Service
