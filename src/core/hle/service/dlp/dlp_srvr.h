// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace DLP {

class DLP_SRVR_Interface final : public Interface {
public:
    DLP_SRVR_Interface();

    std::string GetPortName() const override {
        return "dlp:SRVR";
    }
};

} // namespace DLP
} // namespace Service
