// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included..

#pragma once

#include "core/hle/service/cam/cam.h"

namespace Service::CAM {

class CAM_S final : public Module::Interface {
public:
    explicit CAM_S(std::shared_ptr<Module> cam);
};

} // namespace Service::CAM
