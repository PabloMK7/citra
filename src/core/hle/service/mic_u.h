// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace MIC_U

// mic service

namespace MIC_U {

class Interface : public Service::Interface {
public:
    Interface();
    ~Interface();

    std::string GetPortName() const override {
        return "mic:u";
    }
};

} // namespace
