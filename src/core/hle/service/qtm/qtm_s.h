// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace QTM {

class QTM_S final : public Interface {
public:
    QTM_S();

    std::string GetPortName() const override {
        return "qtm:s";
    }
};

} // namespace QTM
} // namespace Service
