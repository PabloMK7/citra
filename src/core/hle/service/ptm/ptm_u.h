// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/hle/service/ptm/ptm.h"

namespace Service::PTM {

class PTM_U final : public Module::Interface {
public:
    explicit PTM_U(std::shared_ptr<Module> ptm);
};

} // namespace Service::PTM
