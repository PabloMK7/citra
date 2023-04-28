// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/pxi/dev.h"

SERIALIZE_EXPORT_IMPL(Service::PXI::DEV)

namespace Service::PXI {

DEV::DEV() : ServiceFramework("pxi:dev", 1) {
    // clang-format off
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0001, 7, 2), nullptr, "ReadHostIO"},
        {IPC::MakeHeader(0x0002, 7, 2), nullptr, "WriteHostIO"},
        {IPC::MakeHeader(0x0003, 4, 2), nullptr, "ReadHostEx"},
        {IPC::MakeHeader(0x0004, 4, 2), nullptr, "WriteHostEx"},
        {IPC::MakeHeader(0x0005, 4, 2), nullptr, "WriteHostExStart"},
        {IPC::MakeHeader(0x0006, 4, 2), nullptr, "WriteHostExChunk"},
        {IPC::MakeHeader(0x0007, 0, 0), nullptr, "WriteHostExEnd"},
        {IPC::MakeHeader(0x0008, 0, 0), nullptr, "InitializeMIDI"},
        {IPC::MakeHeader(0x0009, 0, 0), nullptr, "FinalizeMIDI"},
        {IPC::MakeHeader(0x000A, 0, 0), nullptr, "GetMIDIInfo"},
        {IPC::MakeHeader(0x000B, 0, 0), nullptr, "GetMIDIBufferSize"},
        {IPC::MakeHeader(0x000C, 1, 2), nullptr, "ReadMIDI"},
        {IPC::MakeHeader(0x000D, 26, 8), nullptr, "SPIMultiWriteRead"},
        {IPC::MakeHeader(0x000E, 10, 4), nullptr, "SPIWriteRead"},
        {IPC::MakeHeader(0x000F, 0, 0), nullptr, "GetCardDevice"},
        // clang-format on
    };
    // clang-format on
    RegisterHandlers(functions);
}

DEV::~DEV() = default;

} // namespace Service::PXI
