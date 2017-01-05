// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>

#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/server_session.h"
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

ResultVal<SharedPtr<ServerSession>> ServerSession::Create(
    std::string name, std::shared_ptr<Service::SessionRequestHandler> hle_handler) {
    SharedPtr<ServerSession> server_session(new ServerSession);

    server_session->name = std::move(name);
    server_session->signaled = false;
    server_session->hle_handler = std::move(hle_handler);
    server_session->parent = nullptr;

    return MakeResult<SharedPtr<ServerSession>>(std::move(server_session));
}

bool ServerSession::ShouldWait(Thread* thread) const {
    return !signaled;
}

void ServerSession::Acquire(Thread* thread) {
    ASSERT_MSG(!ShouldWait(thread), "object unavailable!");
    signaled = false;
}

ResultCode ServerSession::HandleSyncRequest() {
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
    }

    // If this ServerSession does not have an HLE implementation, just wake up the threads waiting
    // on it.
    signaled = true;
    WakeupAllWaitingThreads();
    return RESULT_SUCCESS;
}

ServerSession::SessionPair ServerSession::CreateSessionPair(
    const std::string& name, std::shared_ptr<Service::SessionRequestHandler> hle_handler,
    SharedPtr<ClientPort> port) {

    auto server_session =
        ServerSession::Create(name + "_Server", std::move(hle_handler)).MoveFrom();
    auto client_session = ClientSession::Create(name + "_Client").MoveFrom();

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
}
