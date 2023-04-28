// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/am/am_app.h"

namespace Service::AM {

AM_APP::AM_APP(std::shared_ptr<Module> am) : Module::Interface(std::move(am), "am:app", 5) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x1001, 3, 0), &AM_APP::GetDLCContentInfoCount, "GetDLCContentInfoCount"},
        {IPC::MakeHeader(0x1002, 4, 4), &AM_APP::FindDLCContentInfos, "FindDLCContentInfos"},
        {IPC::MakeHeader(0x1003, 5, 2), &AM_APP::ListDLCContentInfos, "ListDLCContentInfos"},
        {IPC::MakeHeader(0x1004, 4, 2), &AM_APP::DeleteContents, "DeleteContents"},
        {IPC::MakeHeader(0x1005, 2, 4), &AM_APP::GetDLCTitleInfos, "GetDLCTitleInfos"},
        {IPC::MakeHeader(0x1006, 2, 0), nullptr, "GetNumDataTitleTickets"},
        {IPC::MakeHeader(0x1007, 4, 2), &AM_APP::ListDataTitleTicketInfos, "ListDataTitleTicketInfos"},
        {IPC::MakeHeader(0x1008, 7, 2), nullptr, "GetItemRights"},
        {IPC::MakeHeader(0x1009, 3, 0), nullptr, "IsDataTitleInUse"},
        {IPC::MakeHeader(0x100A, 0, 0), nullptr, "IsExternalTitleDatabaseInitialized"},
        {IPC::MakeHeader(0x100B, 3, 0), nullptr, "GetNumExistingContentInfos"},
        {IPC::MakeHeader(0x100C, 5, 2), nullptr, "ListExistingContentInfos"},
        {IPC::MakeHeader(0x100D, 2, 4), &AM_APP::GetPatchTitleInfos, "GetPatchTitleInfos"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::AM

SERIALIZE_EXPORT_IMPL(Service::AM::AM_APP)
