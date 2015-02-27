// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace CFG {

class CFG_U_Interface : public Service::Interface {
public:
    CFG_U_Interface();

    std::string GetPortName() const override {
        return "cfg:u";
    }
};

} // namespace CFG
} // namespace Service
