// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace NFC {

class NFC_M final : public Interface {
public:
    NFC_M();

    std::string GetPortName() const override {
        return "nfc:m";
    }
};

} // namespace NFC
} // namespace Service
