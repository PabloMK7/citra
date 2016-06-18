// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"

#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/service/service.h"

namespace Kernel {

ClientSession::ClientSession() {}
ClientSession::~ClientSession() {}

ResultVal<SharedPtr<ClientSession>> ClientSession::Create(SharedPtr<ServerSession> server_session, SharedPtr<ClientPort> client_port, std::string name) {
    SharedPtr<ClientSession> client_session(new ClientSession);

    client_session->name = std::move(name);
    client_session->server_session = server_session;
    client_session->client_port = client_port;
    client_session->hle_helper = client_port->hle_interface;

    return MakeResult<SharedPtr<ClientSession>>(std::move(client_session));
}

ResultCode ClientSession::HandleSyncRequest() {
    // Signal the server session that new data is available
    ResultCode result = server_session->HandleSyncRequest();

    if (result.IsError())
        return result;

    // If this ClientSession has an associated HLE helper, forward the request to it.
    if (hle_helper != nullptr)
        result = hle_helper->HandleSyncRequest(server_session);

    return result;
}

} // namespace
