// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/apt/ns_s.h"

namespace Service::NS {

NS_S::NS_S(std::shared_ptr<Service::APT::Module> apt)
    : Service::APT::Module::NSInterface(std::move(apt), "ns:s", 2) {
    static const FunctionInfo functions[] = {
        {0x000100C0, nullptr, "LaunchFIRM"},
        {0x000200C0, nullptr, "LaunchTitle"},
        {0x00030000, nullptr, "TerminateApplication"},
        {0x00040040, nullptr, "TerminateProcess"},
        {0x000500C0, nullptr, "LaunchApplicationFIRM"},
        {0x00060042, nullptr, "SetFIRMParams4A0"},
        {0x00070042, nullptr, "CardUpdateInitialize"},
        {0x00080000, nullptr, "CardUpdateShutdown"},
        {0x000D0140, nullptr, "SetTWLBannerHMAC"},
        {0x000E0000, nullptr, "ShutdownAsync"},
        {0x00100180, nullptr, "RebootSystem"},
        {0x00110100, nullptr, "TerminateTitle"},
        {0x001200C0, nullptr, "SetApplicationCpuTimeLimit"},
        {0x00150140, nullptr, "LaunchApplication"},
        {0x00160000, nullptr, "RebootSystemClean"},
    };
    RegisterHandlers(functions);
}

} // namespace Service::NS

SERIALIZE_EXPORT_IMPL(Service::NS::NS_S)
