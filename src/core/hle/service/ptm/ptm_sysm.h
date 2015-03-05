// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace PTM {

class PTM_Sysm_Interface : public Interface {
public:
    PTM_Sysm_Interface();

    std::string GetPortName() const override {
        return "ptm:sysm";
    }
};

} // namespace PTM
} // namespace Service
