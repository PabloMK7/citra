// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace QTM {

class QTM_U final : public Interface {
public:
    QTM_U();

    std::string GetPortName() const override {
        return "qtm:u";
    }
};

} // namespace QTM
} // namespace Service
