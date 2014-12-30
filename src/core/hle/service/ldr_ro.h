// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace LDR_RO

namespace LDR_RO {

class Interface : public Service::Interface {
public:
    Interface();

    std::string GetPortName() const override {
        return "ldr:ro";
    }
};

} // namespace
