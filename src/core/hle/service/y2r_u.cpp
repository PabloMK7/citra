// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"

#include "core/hle/hle.h"
#include "core/hle/kernel/event.h"
#include "core/hle/service/y2r_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace Y2R_U

namespace Y2R_U {

static Kernel::SharedPtr<Kernel::Event> completion_event;

/**
 * Y2R_U::IsBusyConversion service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Whether the current conversion is of type busy conversion (?)
 */
static void IsBusyConversion(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;;
    cmd_buff[2] = 0;

    LOG_WARNING(Service, "(STUBBED) called");
}

/**
 * Y2R_U::GetTransferEndEvent service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      3 : The handle of the completion event
 */
static void GetTransferEndEvent(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[3] = Kernel::g_handle_table.Create(completion_event).MoveFrom();
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010040, nullptr,                 "SetInputFormat"},
    {0x00030040, nullptr,                 "SetOutputFormat"},
    {0x00050040, nullptr,                 "SetRotation"},
    {0x00070040, nullptr,                 "SetBlockAlignment"},
    {0x000D0040, nullptr,                 "SetTransferEndInterrupt"},
    {0x000F0000, GetTransferEndEvent,     "GetTransferEndEvent"},
    {0x00100102, nullptr,                 "SetSendingY"},
    {0x00110102, nullptr,                 "SetSendingU"},
    {0x00120102, nullptr,                 "SetSendingV"},
    {0x00180102, nullptr,                 "SetReceiving"},
    {0x001A0040, nullptr,                 "SetInputLineWidth"},
    {0x001C0040, nullptr,                 "SetInputLines"},
    {0x00200040, nullptr,                 "SetStandardCoefficient"},
    {0x00220040, nullptr,                 "SetAlpha"},
    {0x00260000, nullptr,                 "StartConversion"},
    {0x00270000, nullptr,                 "StopConversion"},
    {0x00280000, IsBusyConversion,        "IsBusyConversion"},
    {0x002A0000, nullptr,                 "PingProcess"},
    {0x002B0000, nullptr,                 "DriverInitialize"},
    {0x002C0000, nullptr,                 "DriverFinalize"}
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    completion_event = Kernel::Event::Create(RESETTYPE_ONESHOT, "Y2R:Completed");

    Register(FunctionTable);
}
    
} // namespace
