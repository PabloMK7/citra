// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/pxi/dev.h"

namespace Service::PXI {

DEV::DEV() : ServiceFramework("pxi:dev", 1) {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0x000101C2, nullptr, "ReadHostIO"},
        {0x000201C2, nullptr, "WriteHostIO"},
        {0x00030102, nullptr, "ReadHostEx"},
        {0x00040102, nullptr, "WriteHostEx"},
        {0x00050102, nullptr, "WriteHostExStart"},
        {0x00060102, nullptr, "WriteHostExChunk"},
        {0x00070000, nullptr, "WriteHostExEnd"},
        {0x00080000, nullptr, "InitializeMIDI"},
        {0x00090000, nullptr, "FinalizeMIDI"},
        {0x000A0000, nullptr, "GetMIDIInfo"},
        {0x000B0000, nullptr, "GetMIDIBufferSize"},
        {0x000C0042, nullptr, "ReadMIDI"},
        {0x000D0688, nullptr, "SPIMultiWriteRead"},
        {0x000E0284, nullptr, "SPIWriteRead"},
        {0x000F0000, nullptr, "GetCardDevice"},
    };
    // clang-format on
    RegisterHandlers(functions);
}

DEV::~DEV() = default;

} // namespace Service::PXI
