// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace FRD_A

namespace FRD_A {

class Interface : public Service::Interface {
public:
    Interface();

    std::string GetPortName() const override {
        return "frd:a";
    }
};

} // namespace
