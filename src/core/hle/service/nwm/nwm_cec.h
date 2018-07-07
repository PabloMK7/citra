// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace NWM {

class NWM_CEC final : public ServiceFramework<NWM_CEC> {
public:
    NWM_CEC();
};

} // namespace NWM
} // namespace Service
