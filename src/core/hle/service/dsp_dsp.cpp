// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/log.h"
#include "core/hle/hle.h"
#include "core/hle/service/dsp_dsp.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace DSP_DSP

namespace DSP_DSP {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010040, nullptr,               "RecvData"},
    {0x00020040, nullptr,               "RecvDataIsReady"},
    {0x00030080, nullptr,               "SendData"},
    {0x00040040, nullptr,               "SendDataIsEmpty"},
    {0x00070040, nullptr,               "WriteReg0x10"},
    {0x00080000, nullptr,               "GetSemaphore"},
    {0x00090040, nullptr,               "ClearSemaphore"},
    {0x000B0000, nullptr,               "CheckSemaphoreRequest"},
    {0x000C0040, nullptr,               "ConvertProcessAddressFromDspDram"},
    {0x000D0082, nullptr,               "WriteProcessPipe"},
    {0x001000C0, nullptr,               "ReadPipeIfPossible"},
    {0x001100C2, nullptr,               "LoadComponent"},
    {0x00120000, nullptr,               "UnloadComponent"},
    {0x00130082, nullptr,               "FlushDataCache"},
    {0x00140082, nullptr,               "InvalidateDCache"},
    {0x00150082, nullptr,               "RegisterInterruptEvents"},
    {0x00160000, nullptr,               "GetSemaphoreEventHandle"},
    {0x00170040, nullptr,               "SetSemaphoreMask"},
    {0x00180040, nullptr,               "GetPhysicalAddress"},
    {0x00190040, nullptr,               "GetVirtualAddress"},
    {0x001A0042, nullptr,               "SetIirFilterI2S1_cmd1"},
    {0x001B0042, nullptr,               "SetIirFilterI2S1_cmd2"},
    {0x001C0082, nullptr,               "SetIirFilterEQ"},
    {0x001F0000, nullptr,               "GetHeadphoneStatus"},
    {0x00210000, nullptr,               "GetIsDspOccupied"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

Interface::~Interface() {
}

} // namespace
