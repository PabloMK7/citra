// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace HTTP {

class HTTP_C final : public ServiceFramework<HTTP_C> {
public:
    HTTP_C();
};

void InstallInterfaces(SM::ServiceManager& service_manager);

} // namespace HTTP
} // namespace Service
