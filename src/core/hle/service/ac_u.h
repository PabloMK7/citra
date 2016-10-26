// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace AC_U

// socket service "ac:u"

namespace AC_U {

class Interface : public Service::Interface {
public:
    Interface();
    ~Interface();

    std::string GetPortName() const override {
        return "ac:u";
    }
};

} // namespace
