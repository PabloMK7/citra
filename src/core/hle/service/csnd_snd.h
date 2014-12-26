// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace CSND_SND

namespace CSND_SND {

class Interface : public Service::Interface {
public:
    Interface();

    std::string GetPortName() const override {
        return "csnd:SND";
    }
};

} // namespace
