// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/am_sys.h"

namespace Service {
namespace AM {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010040, TitleIDListGetTotal,         "TitleIDListGetTotal"},
    {0x00020082, GetTitleIDList,              "GetTitleIDList"},
    {0x00030084, nullptr,                     "ListTitles"},
    {0x000400C0, nullptr,                     "DeleteApplicationTitle"},
    {0x000500C0, nullptr,                     "GetTitleProductCode"},
    {0x00080000, nullptr,                     "TitleIDListGetTotal3"},
    {0x00090082, nullptr,                     "GetTitleIDList3"},
    {0x000A0000, nullptr,                     "GetDeviceID"},
    {0x000D0084, nullptr,                     "ListTitles2"},
    {0x00140040, nullptr,                     "FinishInstallToMedia"},
    {0x00180080, nullptr,                     "InitializeTitleDatabase"},
    {0x00190040, nullptr,                     "ReloadDBS"},
    {0x001A00C0, nullptr,                     "GetDSiWareExportSize"},
    {0x001B0144, nullptr,                     "ExportDSiWare"},
    {0x001C0084, nullptr,                     "ImportDSiWare"},
    {0x00230080, nullptr,                     "TitleIDListGetTotal2"},
    {0x002400C2, nullptr,                     "GetTitleIDList2"}
};

AM_SYS_Interface::AM_SYS_Interface() {
    Register(FunctionTable);
}

} // namespace AM
} // namespace Service
