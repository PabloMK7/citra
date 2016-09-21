// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/boss/boss.h"
#include "core/hle/service/boss/boss_p.h"
#include "core/hle/service/boss/boss_u.h"
#include "core/hle/service/service.h"

namespace Service {
namespace BOSS {

void Init() {
    using namespace Kernel;

    AddService(new BOSS_P_Interface);
    AddService(new BOSS_U_Interface);
}

void Shutdown() {}

} // namespace BOSS

} // namespace Service
