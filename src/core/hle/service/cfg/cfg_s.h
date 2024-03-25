// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/cfg/cfg.h"

namespace Service::CFG {

class CFG_S final : public Module::Interface {
public:
    explicit CFG_S(std::shared_ptr<Module> cfg);
};

} // namespace Service::CFG
