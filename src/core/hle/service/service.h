// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <boost/container/flat_map.hpp>
#include "common/common_types.h"
#include "core/hle/ipc.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/result.h"
#include "core/memory.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace Service

namespace Service {

static const int kMaxPortSize = 8; ///< Maximum size of a port name (8 characters)
static const u32 DefaultMaxSessions = 10; ///< Arbitrary default number of maximum connections to an HLE service

/**
 * Interface implemented by HLE Session handlers.
 * This can be provided to a ServerSession in order to hook into several relevant events (such as a new connection or a SyncRequest)
 * so they can be implemented in the emulator.
 */
class SessionRequestHandler {
public:
    /**
     * Dispatches and handles a sync request from the emulated application.
     * @param server_session The ServerSession that was triggered for this sync request,
     * it should be used to differentiate which client (As in ClientSession) we're answering to.
     * @returns ResultCode the result code of the translate operation.
     */
    ResultCode HandleSyncRequest(Kernel::SharedPtr<Kernel::ServerSession> server_session);

    /**
     * Signals that a client has just connected to this HLE handler and keeps the
     * associated ServerSession alive for the duration of the connection.
     * @param server_session Owning pointer to the ServerSession associated with the connection.
     */
    void ClientConnected(Kernel::SharedPtr<Kernel::ServerSession> server_session);

    /**
     * Signals that a client has just disconnected from this HLE handler and releases the
     * associated ServerSession.
     * @param server_session ServerSession associated with the connection.
     */
    void ClientDisconnected(Kernel::SharedPtr<Kernel::ServerSession> server_session);

protected:
    /**
     * Handles a sync request from the emulated application and writes the response to the command buffer.
     * TODO(Subv): Use a wrapper structure to hold all the information relevant to
     * this request (ServerSession, Originator thread, Translated command buffer, etc).
     */
    virtual void HandleSyncRequestImpl(Kernel::SharedPtr<Kernel::ServerSession> server_session) = 0;

private:
    /**
     * Performs command buffer translation for this request.
     * The command buffer from the ServerSession thread's TLS is copied into a
     * buffer and all descriptors in the buffer are processed.
     * TODO(Subv): Implement this function, currently we do not support multiple processes running at once,
     * but once that is implemented we'll need to properly translate all descriptors in the command buffer.
     */
    ResultCode TranslateRequest(Kernel::SharedPtr<Kernel::ServerSession> server_session);

    /// List of sessions that are connected to this handler.
    /// A ServerSession whose server endpoint is an HLE implementation is kept alive by this list for the duration of the connection.
    std::vector<Kernel::SharedPtr<Kernel::ServerSession>> connected_sessions;
};

/**
 * Framework for implementing HLE service handlers which dispatch incoming SyncRequests based on a table mapping header ids to handler functions.
 */
class Interface : public SessionRequestHandler {
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
     * Gets the maximum allowed number of sessions that can be connected to this service at the same time.
     * @returns The maximum number of connections allowed.
     */
    u32 GetMaxSessions() const { return max_sessions; }

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
    void HandleSyncRequestImpl(Kernel::SharedPtr<Kernel::ServerSession> server_session) override;

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
/// Map of services registered with the "srv:" service, retrieved using GetServiceHandle.
extern std::unordered_map<std::string, Kernel::SharedPtr<Kernel::ClientPort>> g_srv_services;

/// Adds a service to the services table
void AddService(Interface* interface_);

} // namespace
