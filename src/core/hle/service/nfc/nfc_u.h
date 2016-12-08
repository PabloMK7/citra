// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace NFC {

class NFC_U final : public Interface {
public:
    NFC_U();

    std::string GetPortName() const override {
        return "nfc:u";
    }
};

} // namespace NFC
} // namespace Service
