// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace FS_User

namespace Service {
namespace FS {

/// Interface to "fs:USER" service
class FSUserInterface : public Service::Interface {
public:

    FSUserInterface();

    ~FSUserInterface();

    /**
     * Gets the string port name used by CTROS for the service
     * @return Port name of service
     */
    std::string GetPortName() const override {
        return "fs:USER";
    }
};

} // namespace FS
} // namespace Service
