// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included..

#pragma once

#include "core/hle/service/service.h"

namespace Service::CAM {

class CAM_Q : public ServiceFramework<CAM_Q> {
public:
    CAM_Q();
};

} // namespace Service::CAM
