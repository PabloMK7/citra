// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace SRV

namespace SRV {

/// Interface to "srv:" service
class Interface : public Service::Interface {
public:
    Interface();
    ~Interface() override;

    std::string GetPortName() const override {
        return "srv:";
    }
};

} // namespace
