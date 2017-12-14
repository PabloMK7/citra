// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include "core/hle/service/ir/ir.h"
#include "core/hle/service/ir/ir_rst.h"
#include "core/hle/service/ir/ir_u.h"
#include "core/hle/service/ir/ir_user.h"
#include "core/hle/service/service.h"

namespace Service {
namespace IR {

static std::weak_ptr<IR_RST> current_ir_rst;
static std::weak_ptr<IR_USER> current_ir_user;

void ReloadInputDevices() {
    if (auto ir_user = current_ir_user.lock())
        ir_user->ReloadInputDevices();

    if (auto ir_rst = current_ir_rst.lock())
        ir_rst->ReloadInputDevices();
}

void InstallInterfaces(SM::ServiceManager& service_manager) {
    std::make_shared<IR_U>()->InstallAsService(service_manager);

    auto ir_user = std::make_shared<IR_USER>();
    ir_user->InstallAsService(service_manager);
    current_ir_user = ir_user;

    auto ir_rst = std::make_shared<IR_RST>();
    ir_rst->InstallAsService(service_manager);
    current_ir_rst = ir_rst;
}

} // namespace IR

} // namespace Service
