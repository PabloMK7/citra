// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::PM {

class PM_APP final : public ServiceFramework<PM_APP> {
public:
    PM_APP();
    ~PM_APP() = default;
};

} // namespace Service::PM
