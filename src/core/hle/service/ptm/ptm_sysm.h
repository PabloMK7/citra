// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/hle/service/ptm/ptm.h"

namespace Service::PTM {

class PTM_S_Common : public Module::Interface {
public:
    explicit PTM_S_Common(std::shared_ptr<Module> ptm, const char* name);
};

class PTM_S final : public PTM_S_Common {
public:
    explicit PTM_S(std::shared_ptr<Module> ptm);
};

class PTM_Sysm final : public PTM_S_Common {
public:
    explicit PTM_Sysm(std::shared_ptr<Module> ptm);
};

} // namespace Service::PTM
