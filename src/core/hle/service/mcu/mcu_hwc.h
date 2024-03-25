// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::MCU {

class HWC final : public ServiceFramework<HWC> {
public:
    explicit HWC();
};

} // namespace Service::MCU
