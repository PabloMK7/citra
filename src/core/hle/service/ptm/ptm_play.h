// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace PTM {

class PTM_Play_Interface : public Service::Interface {
public:
    PTM_Play_Interface();

    std::string GetPortName() const override {
        return "ptm:play";
    }
};

} // namespace PTM
} // namespace Service
