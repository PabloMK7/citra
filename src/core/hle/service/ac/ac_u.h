// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace AC {

class AC_U final : public Interface {
public:
    AC_U();

    std::string GetPortName() const override {
        return "ac:u";
    }
};

} // namespace AC
} // namespace Service
