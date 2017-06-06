// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>
#include "core/hle/kernel/kernel.h"

namespace Kernel {

class ServerSession;

/**
 * Interface implemented by HLE Session handlers.
 * This can be provided to a ServerSession in order to hook into several relevant events
 * (such as a new connection or a SyncRequest) so they can be implemented in the emulator.
 */
class SessionRequestHandler : public std::enable_shared_from_this<SessionRequestHandler> {
public:
    virtual ~SessionRequestHandler() = default;

    /**
     * Handles a sync request from the emulated application.
     * @param server_session The ServerSession that was triggered for this sync request,
     * it should be used to differentiate which client (As in ClientSession) we're answering to.
     * TODO(Subv): Use a wrapper structure to hold all the information relevant to
     * this request (ServerSession, Originator thread, Translated command buffer, etc).
     * @returns ResultCode the result code of the translate operation.
     */
    virtual void HandleSyncRequest(SharedPtr<ServerSession> server_session) = 0;

    /**
     * Signals that a client has just connected to this HLE handler and keeps the
     * associated ServerSession alive for the duration of the connection.
     * @param server_session Owning pointer to the ServerSession associated with the connection.
     */
    void ClientConnected(SharedPtr<ServerSession> server_session);

    /**
     * Signals that a client has just disconnected from this HLE handler and releases the
     * associated ServerSession.
     * @param server_session ServerSession associated with the connection.
     */
    void ClientDisconnected(SharedPtr<ServerSession> server_session);

protected:
    /// List of sessions that are connected to this handler.
    /// A ServerSession whose server endpoint is an HLE implementation is kept alive by this list
    // for the duration of the connection.
    std::vector<SharedPtr<ServerSession>> connected_sessions;
};

} // namespace Kernel
