// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <random>
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::SSL {

class SSL_C final : public ServiceFramework<SSL_C> {
public:
    SSL_C();

private:
    void Initialize(Kernel::HLERequestContext& ctx);
    void GenerateRandomData(Kernel::HLERequestContext& ctx);
};

void InstallInterfaces(Core::System& system);

void GenerateRandomData(std::vector<u8>& out);

} // namespace Service::SSL
