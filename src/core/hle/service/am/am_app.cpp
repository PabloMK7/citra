// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/am/am.h"
#include "core/hle/service/am/am_app.h"

namespace Service {
namespace AM {

const Interface::FunctionInfo FunctionTable[] = {
    {0x100100C0, GetNumContentInfos, "GetNumContentInfos"},
    {0x10020104, FindContentInfos, "FindContentInfos"},
    {0x10030142, ListContentInfos, "ListContentInfos"},
    {0x10040102, DeleteContents, "DeleteContents"},
    {0x10050084, GetDataTitleInfos, "GetDataTitleInfos"},
    {0x10070102, ListDataTitleTicketInfos, "ListDataTitleTicketInfos"},
    {0x100900C0, nullptr, "IsDataTitleInUse"},
    {0x100A0000, nullptr, "IsExternalTitleDatabaseInitialized"},
};

AM_APP_Interface::AM_APP_Interface() {
    Register(FunctionTable);
}

} // namespace AM
} // namespace Service
