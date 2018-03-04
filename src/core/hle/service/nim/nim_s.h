// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included..

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace NIM {

class NIM_S final : public ServiceFramework<NIM_S> {
public:
    NIM_S();
    ~NIM_S();
};

} // namespace NIM
} // namespace Service
