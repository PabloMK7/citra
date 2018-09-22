// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::PM {

class PM_DBG final : public ServiceFramework<PM_DBG> {
public:
    PM_DBG();
    ~PM_DBG() = default;
};

} // namespace Service::PM
