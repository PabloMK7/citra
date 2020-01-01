// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>
#include <boost/serialization/unordered_map.hpp>
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::SOC {

/// Holds information about a particular socket
struct SocketHolder {
    u32 socket_fd; ///< The socket descriptor
    bool blocking; ///< Whether the socket is blocking or not, it is only read on Windows.

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& socket_fd;
        ar& blocking;
    }
    friend class boost::serialization::access;
};

class SOC_U final : public ServiceFramework<SOC_U> {
public:
    SOC_U();
    ~SOC_U();

private:
    void Socket(Kernel::HLERequestContext& ctx);
    void Bind(Kernel::HLERequestContext& ctx);
    void Fcntl(Kernel::HLERequestContext& ctx);
    void Listen(Kernel::HLERequestContext& ctx);
    void Accept(Kernel::HLERequestContext& ctx);
    void GetHostId(Kernel::HLERequestContext& ctx);
    void Close(Kernel::HLERequestContext& ctx);
    void SendTo(Kernel::HLERequestContext& ctx);
    void RecvFromOther(Kernel::HLERequestContext& ctx);
    void RecvFrom(Kernel::HLERequestContext& ctx);
    void Poll(Kernel::HLERequestContext& ctx);
    void GetSockName(Kernel::HLERequestContext& ctx);
    void Shutdown(Kernel::HLERequestContext& ctx);
    void GetPeerName(Kernel::HLERequestContext& ctx);
    void Connect(Kernel::HLERequestContext& ctx);
    void InitializeSockets(Kernel::HLERequestContext& ctx);
    void ShutdownSockets(Kernel::HLERequestContext& ctx);
    void GetSockOpt(Kernel::HLERequestContext& ctx);
    void SetSockOpt(Kernel::HLERequestContext& ctx);

    // Some platforms seem to have GetAddrInfo and GetNameInfo defined as macros,
    // so we have to use a different name here.
    void GetAddrInfoImpl(Kernel::HLERequestContext& ctx);
    void GetNameInfoImpl(Kernel::HLERequestContext& ctx);

    /// Close all open sockets
    void CleanupSockets();

    /// Holds info about the currently open sockets
    std::unordered_map<u32, SocketHolder> open_sockets;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);
        ar& open_sockets;
    }
    friend class boost::serialization::access;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::SOC

BOOST_CLASS_EXPORT_KEY(Service::SOC::SOC_U)
