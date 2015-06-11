// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace NEWS {

class NEWS_U_Interface : public Service::Interface {
public:
    NEWS_U_Interface();

    std::string GetPortName() const override {
        return "news:u";
    }
};

} // namespace NEWS
} // namespace Service
