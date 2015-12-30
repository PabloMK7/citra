// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/am_u.h"

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
    {0x002400C2, nullptr,                     "GetTitleIDList2"},
    {0x04010080, nullptr,                     "InstallFIRM"},
    {0x04020040, nullptr,                     "StartInstallCIADB0"},
    {0x04030000, nullptr,                     "StartInstallCIADB1"},
    {0x04040002, nullptr,                     "AbortCIAInstall"},
    {0x04050002, nullptr,                     "CloseCIAFinalizeInstall"},
    {0x04060002, nullptr,                     "CloseCIA"},
    {0x040700C2, nullptr,                     "FinalizeTitlesInstall"},
    {0x04080042, nullptr,                     "GetCiaFileInfo"},
    {0x040E00C2, nullptr,                     "InstallTitlesFinish"},
    {0x040F0000, nullptr,                     "InstallNATIVEFIRM"},
    {0x041000C0, nullptr,                     "DeleteTitle"},
    {0x04120000, nullptr,                     "Initialize"},
    {0x041700C0, nullptr,                     "MigrateAGBtoSAV"}
};

AM_U_Interface::AM_U_Interface() {
    Register(FunctionTable);
}

} // namespace AM
} // namespace Service
