// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/apt/apt.h"

namespace Service::APT {

class APT_A final : public Module::APTInterface {
public:
    explicit APT_A(std::shared_ptr<Module> apt);
};

} // namespace Service::APT
