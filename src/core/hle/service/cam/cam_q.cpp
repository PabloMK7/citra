// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/cam/cam_q.h"

namespace Service::CAM {

CAM_Q::CAM_Q() : ServiceFramework("cam:q", 1 /*TODO: find the true value*/) {
    // Empty arrays are illegal -- commented out until an entry is added.
    // static const FunctionInfo functions[] = {};
    // RegisterHandlers(functions);
}

} // namespace Service::CAM
