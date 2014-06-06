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
    {0x000A0000, nullptr, "GetIPCHandles"},
    {0x00110000, nullptr, "EnableAccelerometer"},
    {0x00130000, nullptr, "EnableGyroscopeLow"},
    {0x00150000, nullptr, "GetGyroscopeLowRawToDpsCoefficient"},
    {0x00160000, nullptr, "GetGyroscopeLowCalibrateParam"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

Interface::~Interface() {
}

} // namespace
