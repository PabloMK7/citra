// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/kernel/kernel.h"
#include "core/hle/service/service.h"

namespace Service {
namespace PXI {

/// Interface to "pxi:dev" service
class DEV final : public ServiceFramework<DEV> {
public:
    DEV();
    ~DEV();
};

} // namespace PXI
} // namespace Service
