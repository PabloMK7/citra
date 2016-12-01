// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>

#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

ServerSession::ServerSession() {}
ServerSession::~ServerSession() {}

ResultVal<SharedPtr<ServerSession>> ServerSession::Create(std::string name, std::shared_ptr<Service::SessionRequestHandler> hle_handler) {
    SharedPtr<ServerSession> server_session(new ServerSession);

    server_session->name = std::move(name);
    server_session->signaled = false;
    server_session->hle_handler = hle_handler;

    return MakeResult<SharedPtr<ServerSession>>(std::move(server_session));
}

bool ServerSession::ShouldWait() {
    return !signaled;
}

void ServerSession::Acquire() {
    ASSERT_MSG(!ShouldWait(), "object unavailable!");
    signaled = false;
}

ResultCode ServerSession::HandleSyncRequest() {
    // The ServerSession received a sync request, this means that there's new data available
    // from its ClientSession, so wake up any threads that may be waiting on a svcReplyAndReceive or similar.

    // If this ServerSession has an associated HLE handler, forward the request to it.
    if (hle_handler != nullptr)
        return hle_handler->HandleSyncRequest(SharedPtr<ServerSession>(this));

    // If this ServerSession does not have an HLE implementation, just wake up the threads waiting on it.
    signaled = true;
    WakeupAllWaitingThreads();
    return RESULT_SUCCESS;
}

std::tuple<SharedPtr<ServerSession>, SharedPtr<ClientSession>> ServerSession::CreateSessionPair(const std::string& name, std::shared_ptr<Service::SessionRequestHandler> hle_handler) {
    auto server_session = ServerSession::Create(name + "Server", hle_handler).MoveFrom();
    auto client_session = ClientSession::Create(server_session, name + "Client").MoveFrom();

    return std::make_tuple(server_session, client_session);
}

}
