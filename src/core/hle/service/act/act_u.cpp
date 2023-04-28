// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/act/act_u.h"

namespace Service::ACT {

ACT_U::ACT_U(std::shared_ptr<Module> act) : Module::Interface(std::move(act), "act:u") {
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0001, 2, 4), nullptr, "Initialize"},
        {IPC::MakeHeader(0x0002, 1, 0), nullptr, "GetErrorCode"},
        {IPC::MakeHeader(0x0006, 3, 2), nullptr, "GetAccountDataBlock"},
        {IPC::MakeHeader(0x000B, 1, 2), nullptr, "AcquireEulaList"},
        {IPC::MakeHeader(0x000D, 1, 0), nullptr, "GenerateUuid"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::ACT

SERIALIZE_EXPORT_IMPL(Service::ACT::ACT_U)
