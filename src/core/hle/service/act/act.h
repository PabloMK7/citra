// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::ACT {

/// Initializes all ACT services
class Module final {
public:
    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> act, const char* name);
        ~Interface();

    private:
        std::shared_ptr<Module> act;
    };
};

void InstallInterfaces(Core::System& system);

} // namespace Service::ACT
