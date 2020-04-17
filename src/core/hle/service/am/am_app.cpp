// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/am/am_app.h"

namespace Service::AM {

AM_APP::AM_APP(std::shared_ptr<Module> am) : Module::Interface(std::move(am), "am:app", 5) {
    static const FunctionInfo functions[] = {
        {0x100100C0, &AM_APP::GetDLCContentInfoCount, "GetDLCContentInfoCount"},
        {0x10020104, &AM_APP::FindDLCContentInfos, "FindDLCContentInfos"},
        {0x10030142, &AM_APP::ListDLCContentInfos, "ListDLCContentInfos"},
        {0x10040102, &AM_APP::DeleteContents, "DeleteContents"},
        {0x10050084, &AM_APP::GetDLCTitleInfos, "GetDLCTitleInfos"},
        {0x10060080, nullptr, "GetNumDataTitleTickets"},
        {0x10070102, &AM_APP::ListDataTitleTicketInfos, "ListDataTitleTicketInfos"},
        {0x100801C2, nullptr, "GetItemRights"},
        {0x100900C0, nullptr, "IsDataTitleInUse"},
        {0x100A0000, nullptr, "IsExternalTitleDatabaseInitialized"},
        {0x100B00C0, nullptr, "GetNumExistingContentInfos"},
        {0x100C0142, nullptr, "ListExistingContentInfos"},
        {0x100D0084, &AM_APP::GetPatchTitleInfos, "GetPatchTitleInfos"},
    };
    RegisterHandlers(functions);
}

} // namespace Service::AM

SERIALIZE_EXPORT_IMPL(Service::AM::AM_APP)
