// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/act/act.h"

namespace Service::ACT {

class ACT_U final : public Module::Interface {
public:
    explicit ACT_U(std::shared_ptr<Module> act);
};

} // namespace Service::ACT
