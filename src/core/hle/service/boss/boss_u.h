// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included..

#pragma once

#include "core/hle/service/boss/boss.h"

namespace Service::BOSS {

class BOSS_U final : public Module::Interface {
public:
    explicit BOSS_U(std::shared_ptr<Module> boss);
};

} // namespace Service::BOSS
