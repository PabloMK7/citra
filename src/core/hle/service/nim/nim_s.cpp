// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/nim/nim_s.h"

SERIALIZE_EXPORT_IMPL(Service::NIM::NIM_S)

namespace Service::NIM {

NIM_S::NIM_S() : ServiceFramework("nim:s", 1) {
    const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x000A, 0, 0), nullptr, "CheckSysupdateAvailableSOAP"},
        {IPC::MakeHeader(0x0016, 8, 10), nullptr, "ListTitles"},
        {IPC::MakeHeader(0x0029, 0, 0), nullptr, "AccountCheckBalanceSOAP"},
        {IPC::MakeHeader(0x002D, 1, 2), nullptr, "DownloadTickets"},
        {IPC::MakeHeader(0x0042, 9, 0), nullptr, "StartDownload"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

NIM_S::~NIM_S() = default;

} // namespace Service::NIM
