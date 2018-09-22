// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::NWM {

class NWM_SOC final : public ServiceFramework<NWM_SOC> {
public:
    NWM_SOC();
};

} // namespace Service::NWM
