// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/hle/service/act/act.h"
#include "core/hle/service/act/act_a.h"
#include "core/hle/service/act/act_u.h"

namespace Service::ACT {

Module::Interface::Interface(std::shared_ptr<Module> act, const char* name)
    : ServiceFramework(name, 1 /* Placeholder */), act(std::move(act)) {}

Module::Interface::~Interface() = default;

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    auto act = std::make_shared<Module>();
    std::make_shared<ACT_A>(act)->InstallAsService(service_manager);
    std::make_shared<ACT_U>(act)->InstallAsService(service_manager);
}

} // namespace Service::ACT
