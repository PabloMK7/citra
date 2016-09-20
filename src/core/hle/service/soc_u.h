// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace SOC_U

namespace SOC_U {

class Interface : public Service::Interface {
public:
    Interface();
    ~Interface();

    std::string GetPortName() const override {
        return "soc:U";
    }
};

} // namespace
