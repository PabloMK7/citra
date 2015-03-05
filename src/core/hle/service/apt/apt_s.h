// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace APT {

// Application and title launching service. These services handle signaling for home/power button as
// well. Only one session for either APT service can be open at a time, normally processes close the
// service handle immediately once finished using the service. The commands for APT:U and APT:S are
// exactly the same, however certain commands are only accessible with APT:S(NS module will call
// svcBreak when the command isn't accessible). See http://3dbrew.org/wiki/NS#APT_Services.

/// Interface to "APT:S" service
class APT_S_Interface : public Service::Interface {
public:
    APT_S_Interface();

    std::string GetPortName() const override {
        return "APT:S";
    }
};

} // namespace APT
} // namespace Service