// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/news/news.h"
#include "core/hle/service/news/news_s.h"

namespace Service {
namespace NEWS {

const Interface::FunctionInfo FunctionTable[] = {
    {0x000100C6, nullptr, "AddNotification"},
    {0x00050000, nullptr, "GetTotalNotifications"},
    {0x00060042, nullptr, "SetNewsDBHeader"},
    {0x00070082, nullptr, "SetNotificationHeader"},
    {0x00080082, nullptr, "SetNotificationMessage"},
    {0x00090082, nullptr, "SetNotificationImage"},
    {0x000A0042, nullptr, "GetNewsDBHeader"},
    {0x000B0082, nullptr, "GetNotificationHeader"},
    {0x000C0082, nullptr, "GetNotificationMessage"},
    {0x000D0082, nullptr, "GetNotificationImage"},
    {0x000E0040, nullptr, "SetInfoLEDPattern"},
    {0x00120082, nullptr, "GetNotificationHeaderOther"},
    {0x00130000, nullptr, "WriteNewsDBSavedata"},
};

NEWS_S_Interface::NEWS_S_Interface() {
    Register(FunctionTable);
}

} // namespace NEWS
} // namespace Service
