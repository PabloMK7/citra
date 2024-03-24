// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::DLP {

class DLP_CLNT final : public ServiceFramework<DLP_CLNT> {
public:
    DLP_CLNT();
    ~DLP_CLNT() = default;
};

} // namespace Service::DLP
