// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace CFG_S

namespace CFG_S {

class Interface : public Service::Interface {
public:
    Interface();

    std::string GetPortName() const override {
        return "cfg:s";
    }
};

} // namespace
