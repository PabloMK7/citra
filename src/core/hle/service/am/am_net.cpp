// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/am_net.h"

namespace Service {
namespace AM {

const Interface::FunctionInfo FunctionTable[] = {
    {0x08010000, nullptr,               "OpenTicket"},
    {0x08020002, nullptr,               "TicketAbortInstall"},
    {0x08030002, nullptr,               "TicketFinalizeInstall"},
    {0x08040100, nullptr,               "InstallTitleBegin"},
    {0x08050000, nullptr,               "InstallTitleAbort"},
    {0x080600C0, nullptr,               "InstallTitleResume"},
    {0x08070000, nullptr,               "InstallTitleAbortTMD"},
    {0x08080000, nullptr,               "InstallTitleFinish"},
    {0x080A0000, nullptr,               "OpenTMD"},
    {0x080B0002, nullptr,               "TMDAbortInstall"},
    {0x080C0042, nullptr,               "TMDFinalizeInstall"},
    {0x080E0040, nullptr,               "OpenContentCreate"},
    {0x080F0002, nullptr,               "ContentAbortInstall"},
    {0x08100040, nullptr,               "OpenContentResume"},
    {0x08120002, nullptr,               "ContentFinalizeInstall"},
    {0x08130000, nullptr,               "GetTotalContents"},
    {0x08140042, nullptr,               "GetContentIndexes"},
    {0x08150044, nullptr,               "GetContentsInfo"},
    {0x08180042, nullptr,               "GetCTCert"},
    {0x08190108, nullptr,               "SetCertificates"},
    {0x081B00C2, nullptr,               "InstallTitlesFinish"},
};

AM_NET_Interface::AM_NET_Interface() {
    Register(FunctionTable);
}

} // namespace AM
} // namespace Service
