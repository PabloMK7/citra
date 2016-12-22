// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace CSND {

class CSND_SND final : public Interface {
public:
    CSND_SND();

    std::string GetPortName() const override {
        return "csnd:SND";
    }
};

} // namespace CSND
} // namespace Service
