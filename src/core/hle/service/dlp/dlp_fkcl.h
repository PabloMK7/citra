// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::DLP {

class DLP_FKCL final : public ServiceFramework<DLP_FKCL> {
public:
    DLP_FKCL();
    ~DLP_FKCL() = default;
};

} // namespace Service::DLP
