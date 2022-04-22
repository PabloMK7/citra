// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/frontend/mic.h"

namespace Mic {

class AndroidFactory final : public Frontend::Mic::RealMicFactory {
public:
    ~AndroidFactory() override;

    std::unique_ptr<Frontend::Mic::Interface> Create(std::string mic_device_name) override;

private:
    bool permission_granted = false;
};

} // namespace Mic
