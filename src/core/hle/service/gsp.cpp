// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#include "common/log.h"

#include "core/hle/hle.h"
#include "core/hle/service/gsp.h"

namespace GSP_GPU {

const HLE::FunctionDef FunctionTable[] = {
    {0x00010082, NULL, "WriteHWRegs"},
    {0x00020084, NULL, "WriteHWRegsWithMask"},
    {0x00030082, NULL, "WriteHWRegRepeat"},
    {0x00040080, NULL, "ReadHWRegs"},
    {0x00050200, NULL, "SetBufferSwap"},
    {0x00060082, NULL, "SetCommandList"},
    {0x000700C2, NULL, "RequestDma"},
    {0x00080082, NULL, "FlushDataCache"},
    {0x00090082, NULL, "InvalidateDataCache"},
    {0x000A0044, NULL, "RegisterInterruptEvents"},
    {0x000B0040, NULL, "SetLcdForceBlack"},
    {0x000C0000, NULL, "TriggerCmdReqQueue"},
    {0x000D0140, NULL, "SetDisplayTransfer"},
    {0x000E0180, NULL, "SetTextureCopy"},
    {0x000F0200, NULL, "SetMemoryFill"},
    {0x00100040, NULL, "SetAxiConfigQoSMode"},
    {0x00110040, NULL, "SetPerfLogMode"},
    {0x00120000, NULL, "GetPerfLog"},
    {0x00130042, NULL, "RegisterInterruptRelayQueue"},
    {0x00140000, NULL, "UnregisterInterruptRelayQueue"},
    {0x00150002, NULL, "TryAcquireRight"},
    {0x00160042, NULL, "AcquireRight"},
    {0x00170000, NULL, "ReleaseRight"},
    {0x00180000, NULL, "ImportDisplayCaptureInfo"},
    {0x00190000, NULL, "SaveVramSysArea"},
    {0x001A0000, NULL, "RestoreVramSysArea"},
    {0x001B0000, NULL, "ResetGpuCore"},
    {0x001C0040, NULL, "SetLedForceOff"},
    {0x001D0040, NULL, "SetTestCommand"},
    {0x001E0080, NULL, "SetInternalPriorities"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

Interface::~Interface() {
}

} // namespace
