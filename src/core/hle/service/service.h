// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <boost/container/flat_map.hpp>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/kernel.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace Service

namespace Kernel {
class ClientPort;
class ServerSession;
}

namespace Service {

static const int kMaxPortSize = 8; ///< Maximum size of a port name (8 characters)
/// Arbitrary default number of maximum connections to an HLE service.
static const u32 DefaultMaxSessions = 10;

/**
 * Framework for implementing HLE service handlers which dispatch incoming SyncRequests based on a
 * table mapping header ids to handler functions.
 */
class Interface : public Kernel::SessionRequestHandler {
public:
    /**
     * Creates an HLE interface with the specified max sessions.
     * @param max_sessions Maximum number of sessions that can be
     * connected to this service at the same time.
     */
    Interface(u32 max_sessions = DefaultMaxSessions);

    virtual ~Interface();

    std::string GetName() const {
        return GetPortName();
    }

    virtual void SetVersion(u32 raw_version) {
        version.raw = raw_version;
    }

    /**
     * Gets the maximum allowed number of sessions that can be connected to this service
     * at the same time.
     * @returns The maximum number of connections allowed.
     */
    u32 GetMaxSessions() const {
        return max_sessions;
    }

    typedef void (*Function)(Interface*);

    struct FunctionInfo {
        u32 id;
        Function func;
        const char* name;
    };

    /**
     * Gets the string name used by CTROS for a service
     * @return Port name of service
     */
    virtual std::string GetPortName() const {
        return "[UNKNOWN SERVICE PORT]";
    }

protected:
    void HandleSyncRequest(Kernel::SharedPtr<Kernel::ServerSession> server_session) override;

    /**
     * Registers the functions in the service
     */
    template <size_t N>
    inline void Register(const FunctionInfo (&functions)[N]) {
        Register(functions, N);
    }

    void Register(const FunctionInfo* functions, size_t n);

    union {
        u32 raw;
        BitField<0, 8, u32> major;
        BitField<8, 8, u32> minor;
        BitField<16, 8, u32> build;
        BitField<24, 8, u32> revision;
    } version = {};

private:
    u32 max_sessions; ///< Maximum number of concurrent sessions that this service can handle.
    boost::container::flat_map<u32, FunctionInfo> m_functions;
};

/// Initialize ServiceManager
void Init();

/// Shutdown ServiceManager
void Shutdown();

/// Map of named ports managed by the kernel, which can be retrieved using the ConnectToPort SVC.
extern std::unordered_map<std::string, Kernel::SharedPtr<Kernel::ClientPort>> g_kernel_named_ports;

/// Adds a service to the services table
void AddService(Interface* interface_);

} // namespace
