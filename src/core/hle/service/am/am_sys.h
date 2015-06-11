// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included..

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace AM {

class AM_SYS_Interface : public Service::Interface {
public:
    AM_SYS_Interface();

    std::string GetPortName() const override {
        return "am:sys";
    }
};

} // namespace AM
} // namespace Service
