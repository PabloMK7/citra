// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/act/act_a.h"

namespace Service::ACT {

ACT_A::ACT_A(std::shared_ptr<Module> act) : Module::Interface(std::move(act), "act:a") {
    const FunctionInfo functions[] = {
        // act:u shared commands
        {0x00010084, nullptr, "Initialize"},
        {0x00020040, nullptr, "GetErrorCode"},
        {0x000600C2, nullptr, "GetAccountDataBlock"},
        {0x000B0042, nullptr, "AcquireEulaList"},
        {0x000D0040, nullptr, "GenerateUuid"},
        // act:a
        {0x041300C2, nullptr, "UpdateMiiImage"},
        {0x041B0142, nullptr, "AgreeEula"},
        {0x04210042, nullptr, "UploadMii"},
        {0x04230082, nullptr, "ValidateMailAddress"},
    };
    RegisterHandlers(functions);
}

} // namespace Service::ACT
