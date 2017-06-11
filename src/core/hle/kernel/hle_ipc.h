// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <vector>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/ipc.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/server_session.h"

namespace Service {
class ServiceFrameworkBase;
}

namespace Kernel {

class HandleTable;
class Process;

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

/**
 * Class containing information about an in-flight IPC request being handled by an HLE service
 * implementation. Services should avoid using old global APIs (e.g. Kernel::GetCommandBuffer()) and
 * when possible use the APIs in this class to service the request.
 *
 * HLE handle protocol
 * ===================
 *
 * To avoid needing HLE services to keep a separate handle table, or having to directly modify the
 * requester's table, a tweaked protocol is used to receive and send handles in requests. The kernel
 * will decode the incoming handles into object pointers and insert a id in the buffer where the
 * handle would normally be. The service then calls GetIncomingHandle() with that id to get the
 * pointer to the object. Similarly, instead of inserting a handle into the command buffer, the
 * service calls AddOutgoingHandle() and stores the returned id where the handle would normally go.
 *
 * The end result is similar to just giving services their own real handle tables, but since these
 * ids are local to a specific context, it avoids requiring services to manage handles for objects
 * across multiple calls and ensuring that unneeded handles are cleaned up.
 */
class HLERequestContext {
public:
    ~HLERequestContext();

    /// Returns a pointer to the IPC command buffer for this request.
    u32* CommandBuffer() {
        return cmd_buf.data();
    }

    /**
     * Returns the session through which this request was made. This can be used as a map key to
     * access per-client data on services.
     */
    SharedPtr<ServerSession> Session() const {
        return session;
    }

    /**
     * Resolves a object id from the request command buffer into a pointer to an object. See the
     * "HLE handle protocol" section in the class documentation for more details.
     */
    SharedPtr<Object> GetIncomingHandle(u32 id_from_cmdbuf) const;

    /**
     * Adds an outgoing object to the response, returning the id which should be used to reference
     * it. See the "HLE handle protocol" section in the class documentation for more details.
     */
    u32 AddOutgoingHandle(SharedPtr<Object> object);

    /**
     * Discards all Objects from the context, invalidating all ids. This may be called after reading
     * out all incoming objects, so that the buffer memory can be re-used for outgoing handles, but
     * this is not required.
     */
    void ClearIncomingObjects();

private:
    friend class Service::ServiceFrameworkBase;

    ResultCode PopulateFromIncomingCommandBuffer(const u32_le* src_cmdbuf, Process& src_process,
                                                 HandleTable& src_table);
    ResultCode WriteToOutgoingCommandBuffer(u32_le* dst_cmdbuf, Process& dst_process,
                                            HandleTable& dst_table) const;

    std::array<u32, IPC::COMMAND_BUFFER_LENGTH> cmd_buf;
    SharedPtr<ServerSession> session;
    std::vector<SharedPtr<Object>> request_handles;
};

} // namespace Kernel
