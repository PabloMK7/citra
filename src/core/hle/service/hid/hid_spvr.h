// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/hid/hid.h"

namespace Service::HID {

class Spvr final : public Module::Interface {
public:
    explicit Spvr(std::shared_ptr<Module> hid);
};

} // namespace Service::HID
