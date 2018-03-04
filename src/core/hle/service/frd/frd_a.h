// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/frd/frd.h"

namespace Service {
namespace FRD {

class FRD_A final : public Module::Interface {
public:
    explicit FRD_A(std::shared_ptr<Module> frd);
};

} // namespace FRD
} // namespace Service
