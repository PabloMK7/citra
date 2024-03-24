// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/frd/frd.h"

namespace Service::FRD {

class FRD_U final : public Module::Interface {
public:
    explicit FRD_U(std::shared_ptr<Module> frd);
};

} // namespace Service::FRD
