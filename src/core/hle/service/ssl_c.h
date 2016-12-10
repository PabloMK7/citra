// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace SSL {

class SSL_C final : public Interface {
public:
    SSL_C();

    std::string GetPortName() const override {
        return "ssl:C";
    }
};

} // namespace SSL
} // namespace Service
