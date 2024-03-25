// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included..

#pragma once

#include "core/hle/service/am/am.h"

namespace Service::AM {

class AM_SYS final : public Module::Interface {
public:
    explicit AM_SYS(std::shared_ptr<Module> am);
};

} // namespace Service::AM
