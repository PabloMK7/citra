// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included..

#pragma once

#include "core/hle/service/cam/cam.h"

namespace Service::CAM {

class CAM_C final : public Module::Interface {
public:
    explicit CAM_C(std::shared_ptr<Module> cam);
};

} // namespace Service::CAM
