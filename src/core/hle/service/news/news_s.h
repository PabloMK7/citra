// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace NEWS {

class NEWS_S_Interface : public Service::Interface {
public:
    NEWS_S_Interface();

    std::string GetPortName() const override {
        return "news:s";
    }
};

} // namespace NEWS
} // namespace Service
