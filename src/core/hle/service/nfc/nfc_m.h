// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/nfc/nfc.h"

namespace Service::NFC {

class NFC_M final : public Module::Interface {
public:
    explicit NFC_M(std::shared_ptr<Module> nfc);
};

} // namespace Service::NFC
