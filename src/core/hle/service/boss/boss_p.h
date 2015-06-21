// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included..

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace BOSS {

class BOSS_P_Interface : public Service::Interface {
public:
    BOSS_P_Interface();

    std::string GetPortName() const override {
        return "boss:P";
    }
};

} // namespace BOSS
} // namespace Service
