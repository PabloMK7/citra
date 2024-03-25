// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::IR {

/// Interface to "ir:u" service
class IR_U final : public ServiceFramework<IR_U> {
public:
    IR_U();
};

} // namespace Service::IR
