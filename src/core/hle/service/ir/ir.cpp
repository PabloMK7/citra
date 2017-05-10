// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/ir/ir.h"
#include "core/hle/service/ir/ir_rst.h"
#include "core/hle/service/ir/ir_u.h"
#include "core/hle/service/ir/ir_user.h"
#include "core/hle/service/service.h"

namespace Service {
namespace IR {

void Init() {
    AddService(new IR_RST_Interface);
    AddService(new IR_U_Interface);
    AddService(new IR_User_Interface);

    InitUser();
    InitRST();
}

void Shutdown() {
    ShutdownUser();
    ShutdownRST();
}

void ReloadInputDevices() {
    ReloadInputDevicesUser();
    ReloadInputDevicesRST();
}

} // namespace IR

} // namespace Service
