// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/log.h"

#include "core/hle/hle.h"
#include "core/hle/service/hid.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace HID_User

namespace HID_User {

const Interface::FunctionInfo FunctionTable[] = {
    {0x000A0000, NULL, "GetIPCHandles"},
    {0x00110000, NULL, "EnableAccelerometer"},
    {0x00130000, NULL, "EnableGyroscopeLow"},
    {0x00150000, NULL, "GetGyroscopeLowRawToDpsCoefficient"},
    {0x00160000, NULL, "GetGyroscopeLowCalibrateParam"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

Interface::~Interface() {
}

} // namespace
