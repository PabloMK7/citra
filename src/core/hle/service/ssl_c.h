// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <random>
#include "core/hle/service/service.h"

namespace Service {
namespace SSL {

class SSL_C final : public ServiceFramework<SSL_C> {
public:
    SSL_C();

private:
    void Initialize(Kernel::HLERequestContext& ctx);
    void GenerateRandomData(Kernel::HLERequestContext& ctx);

    // TODO: Implement a proper CSPRNG in the future when actual security is needed
    std::mt19937 rand_gen;
};

void InstallInterfaces(SM::ServiceManager& service_manager);

} // namespace SSL
} // namespace Service
