// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "core/hle/service/service.h"

namespace Service {
namespace SM {

/// Interface to "srv:" service
class SRV final : public Interface {
public:
    SRV();
    ~SRV() override;

    std::string GetPortName() const override {
        return "srv:";
    }
};

} // namespace SM
} // namespace Service
