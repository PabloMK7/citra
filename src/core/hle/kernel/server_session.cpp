// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>

#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/kernel/session.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

ServerSession::ServerSession() = default;
ServerSession::~ServerSession() {
    // This destructor will be called automatically when the last ServerSession handle is closed by
    // the emulated application.

    // Decrease the port's connection count.
    if (parent->port)
        parent->port->active_sessions--;

    // TODO(Subv): Wake up all the ClientSession's waiting threads and set
    // the SendSyncRequest result to 0xC920181A.

    parent->server = nullptr;
}

ResultVal<SharedPtr<ServerSession>> ServerSession::Create(std::string name) {
    SharedPtr<ServerSession> server_session(new ServerSession);

    server_session->name = std::move(name);
    server_session->parent = nullptr;

    return MakeResult(std::move(server_session));
}

bool ServerSession::ShouldWait(Thread* thread) const {
    // Closed sessions should never wait, an error will be returned from svcReplyAndReceive.
    if (parent->client == nullptr)
        return false;
    // Wait if we have no pending requests, or if we're currently handling a request.
    return pending_requesting_threads.empty() || currently_handling != nullptr;
}

void ServerSession::Acquire(Thread* thread) {
    ASSERT_MSG(!ShouldWait(thread), "object unavailable!");
    // We are now handling a request, pop it from the stack.
    // TODO(Subv): What happens if the client endpoint is closed before any requests are made?
    ASSERT(!pending_requesting_threads.empty());
    currently_handling = pending_requesting_threads.back();
    pending_requesting_threads.pop_back();
}

ResultCode ServerSession::HandleSyncRequest(SharedPtr<Thread> thread) {
    // The ServerSession received a sync request, this means that there's new data available
    // from its ClientSession, so wake up any threads that may be waiting on a svcReplyAndReceive or
    // similar.

    // If this ServerSession has an associated HLE handler, forward the request to it.
    if (hle_handler != nullptr) {
        // Attempt to translate the incoming request's command buffer.
        ResultCode result = TranslateHLERequest(this);
        if (result.IsError())
            return result;
        hle_handler->HandleSyncRequest(SharedPtr<ServerSession>(this));
        // TODO(Subv): Translate the response command buffer.
    } else {
        // Add the thread to the list of threads that have issued a sync request with this
        // server.
        pending_requesting_threads.push_back(std::move(thread));
    }

    // If this ServerSession does not have an HLE implementation, just wake up the threads waiting
    // on it.
    WakeupAllWaitingThreads();
    return RESULT_SUCCESS;
}

ServerSession::SessionPair ServerSession::CreateSessionPair(const std::string& name,
                                                            SharedPtr<ClientPort> port) {
    auto server_session = ServerSession::Create(name + "_Server").Unwrap();
    SharedPtr<ClientSession> client_session(new ClientSession);
    client_session->name = name + "_Client";

    std::shared_ptr<Session> parent(new Session);
    parent->client = client_session.get();
    parent->server = server_session.get();
    parent->port = port;

    client_session->parent = parent;
    server_session->parent = parent;

    return std::make_tuple(std::move(server_session), std::move(client_session));
}

ResultCode TranslateHLERequest(ServerSession* server_session) {
    // TODO(Subv): Implement this function once multiple concurrent processes are supported.
    return RESULT_SUCCESS;
}
} // namespace Kernel
