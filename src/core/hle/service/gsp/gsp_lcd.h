// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::GSP {

class GSP_LCD final : public ServiceFramework<GSP_LCD> {
public:
    GSP_LCD();
    ~GSP_LCD() = default;
};

} // namespace Service::GSP
