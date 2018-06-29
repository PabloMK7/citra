// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace MVD {

class MVD_STD final : public ServiceFramework<MVD_STD> {
public:
    MVD_STD();
    ~MVD_STD() = default;
};

} // namespace MVD
} // namespace Service
