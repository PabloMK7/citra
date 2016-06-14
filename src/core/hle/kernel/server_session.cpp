// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>

#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

ServerSession::ServerSession() {}
ServerSession::~ServerSession() {}

ResultVal<SharedPtr<ServerSession>> ServerSession::Create(std::string name) {
    SharedPtr<ServerSession> server_session(new ServerSession);

    server_session->name = std::move(name);
    server_session->signaled = false;

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
    // from one of its ClientSessions, so wake up any threads that may be waiting on a svcReplyAndReceive or similar.
    signaled = true;
    WakeupAllWaitingThreads();
    return RESULT_SUCCESS;
}

SharedPtr<ClientSession> ServerSession::CreateClientSession() {
    // In Citra, some types of ServerSessions (File and Directory sessions) are not created as a pair of Server-Client sessions,
    // but are instead created as a single ServerSession, which then hands over a ClientSession on demand (When opening the File or Directory).
    // The real kernel (Or more specifically, the real FS service) does create the pair of Sessions at the same time (via svcCreateSession), and simply
    // stores the ClientSession until it is needed.
    return ClientSession::Create(SharedPtr<ServerSession>(this), nullptr, name + "Client").MoveFrom();
}

std::tuple<SharedPtr<ServerSession>, SharedPtr<ClientSession>> ServerSession::CreateSessionPair(SharedPtr<ClientPort> client_port, std::string name) {
    auto server_session = ServerSession::Create(name + "Server").MoveFrom();
    auto client_session = ClientSession::Create(server_session, client_port, name + "Client").MoveFrom();

    return std::make_tuple(server_session, client_session);
}

}
