// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/cam/cam.h"
#include "core/hle/service/cam/cam_u.h"

namespace Service {
namespace CAM {

// Empty arrays are illegal -- commented out until an entry is added.
//const Interface::FunctionInfo FunctionTable[] = { };

CAM_U_Interface::CAM_U_Interface() {
    //Register(FunctionTable);
}

} // namespace CAM
} // namespace Service
