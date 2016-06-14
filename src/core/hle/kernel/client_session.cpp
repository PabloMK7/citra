// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"

#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/kernel/kernel.h"

namespace Kernel {

ClientSession::ClientSession() {}
ClientSession::~ClientSession() {}

ResultVal<SharedPtr<ClientSession>> ClientSession::Create(SharedPtr<ServerSession> server_session, SharedPtr<ClientPort> client_port, std::string name) {
    SharedPtr<ClientSession> client_session(new ClientSession);

    client_session->name = std::move(name);
    client_session->server_session = server_session;
    client_session->client_port = client_port;

    return MakeResult<SharedPtr<ClientSession>>(std::move(client_session));
}

ResultCode ClientSession::HandleSyncRequest() {
    // Signal the server session that new data is available
    ResultCode result = server_session->HandleSyncRequest();

    if (result.IsError())
        return result;

    // Tell the client port to handle the request in case it's an HLE service.
    // The client port can be nullptr for port-less sessions (Like for example File and Directory sessions).
    if (client_port != nullptr)
        result = client_port->HandleSyncRequest();

    return result;
}

} // namespace
