// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>
#include <boost/serialization/unordered_map.hpp>
#include "core/hle/result.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::SOC {

/// Holds information about a particular socket
struct SocketHolder {
#ifdef _WIN32
    using SOCKET = unsigned long long;
    SOCKET socket_fd; ///< The socket descriptor
#else
    u32 socket_fd; ///< The socket descriptor
#endif // _WIN32

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
    static constexpr ResultCode ERR_INVALID_HANDLE =
        ResultCode(ErrorDescription::InvalidHandle, ErrorModule::SOC, ErrorSummary::InvalidArgument,
                   ErrorLevel::Permanent);

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

    // Socked ids
    u32 next_socket_id = 3;
    u32 GetNextSocketID() {
        return next_socket_id++;
    }

    // System timer adjust
    u32 timer_adjust_handle;
    void PreTimerAdjust();
    void PostTimerAdjust();

    /// Close all open sockets
    void CleanupSockets();

    /// Holds info about the currently open sockets
    friend struct CTRPollFD;
    std::unordered_map<u32, SocketHolder> open_sockets;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);
        ar& open_sockets;
        ar& timer_adjust_handle;
    }
    friend class boost::serialization::access;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::SOC

BOOST_CLASS_EXPORT_KEY(Service::SOC::SOC_U)
