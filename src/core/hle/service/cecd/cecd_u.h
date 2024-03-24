// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/cecd/cecd.h"

namespace Service::CECD {

class CECD_U final : public Module::Interface {
public:
    explicit CECD_U(std::shared_ptr<Module> cecd);
};

} // namespace Service::CECD
