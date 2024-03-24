// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::DLP {

class DLP_SRVR final : public ServiceFramework<DLP_SRVR> {
public:
    DLP_SRVR();
    ~DLP_SRVR() = default;

private:
    void IsChild(Kernel::HLERequestContext& ctx);
};

} // namespace Service::DLP
