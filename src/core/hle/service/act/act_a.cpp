// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/act/act_a.h"

namespace Service::ACT {

ACT_A::ACT_A(std::shared_ptr<Module> act) : Module::Interface(std::move(act), "act:a") {
    const FunctionInfo functions[] = {
        // act:u shared commands
        // clang-format off
        {IPC::MakeHeader(0x0001, 2, 4), nullptr, "Initialize"},
        {IPC::MakeHeader(0x0002, 1, 0), nullptr, "GetErrorCode"},
        {IPC::MakeHeader(0x0006, 3, 2), nullptr, "GetAccountDataBlock"},
        {IPC::MakeHeader(0x000B, 1, 2), nullptr, "AcquireEulaList"},
        {IPC::MakeHeader(0x000D, 1, 0), nullptr, "GenerateUuid"},
        // act:a
        {IPC::MakeHeader(0x0413, 3, 2), nullptr, "UpdateMiiImage"},
        {IPC::MakeHeader(0x041B, 5, 2), nullptr, "AgreeEula"},
        {IPC::MakeHeader(0x0421, 1, 2), nullptr, "UploadMii"},
        {IPC::MakeHeader(0x0423, 2, 2), nullptr, "ValidateMailAddress"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::ACT

SERIALIZE_EXPORT_IMPL(Service::ACT::ACT_A)
