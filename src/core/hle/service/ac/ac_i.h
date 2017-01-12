// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace AC {

class AC_I final : public Interface {
public:
    AC_I();

    std::string GetPortName() const override {
        return "ac:i";
    }
};

} // namespace AC
} // namespace Service
