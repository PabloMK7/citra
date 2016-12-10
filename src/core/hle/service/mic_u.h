// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace MIC {

class MIC_U final : public Interface {
public:
    MIC_U();
    ~MIC_U();

    std::string GetPortName() const override {
        return "mic:u";
    }
};

} // namespace MIC
} // namespace Service
