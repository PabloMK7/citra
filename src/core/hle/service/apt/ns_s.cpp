// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/apt/ns_s.h"

namespace Service::NS {

NS_S::NS_S(std::shared_ptr<Service::APT::Module> apt)
    : Service::APT::Module::NSInterface(std::move(apt), "ns:s", 3) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0001, 3, 0), nullptr, "LaunchFIRM"},
        {IPC::MakeHeader(0x0002, 3, 0), nullptr, "LaunchTitle"},
        {IPC::MakeHeader(0x0003, 0, 0), nullptr, "TerminateApplication"},
        {IPC::MakeHeader(0x0004, 1, 0), nullptr, "TerminateProcess"},
        {IPC::MakeHeader(0x0005, 3, 0), nullptr, "LaunchApplicationFIRM"},
        {IPC::MakeHeader(0x0006, 1, 2), &NS_S::SetWirelessRebootInfo, "SetWirelessRebootInfo"},
        {IPC::MakeHeader(0x0007, 1, 2), nullptr, "CardUpdateInitialize"},
        {IPC::MakeHeader(0x0008, 0, 0), nullptr, "CardUpdateShutdown"},
        {IPC::MakeHeader(0x000D, 5, 0), nullptr, "SetTWLBannerHMAC"},
        {IPC::MakeHeader(0x000E, 0, 0), &NS_S::ShutdownAsync, "ShutdownAsync"},
        {IPC::MakeHeader(0x0010, 6, 0), &NS_S::RebootSystem, "RebootSystem"},
        {IPC::MakeHeader(0x0011, 4, 0), nullptr, "TerminateTitle"},
        {IPC::MakeHeader(0x0012, 3, 0), nullptr, "SetApplicationCpuTimeLimit"},
        {IPC::MakeHeader(0x0015, 5, 0), nullptr, "LaunchApplication"},
        {IPC::MakeHeader(0x0016, 0, 0), &NS_S::RebootSystemClean, "RebootSystemClean"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::NS

SERIALIZE_EXPORT_IMPL(Service::NS::NS_S)
