// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"

#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/server_session.h"

namespace Kernel {

ClientSession::ClientSession() = default;
ClientSession::~ClientSession() = default;

ResultVal<SharedPtr<ClientSession>> ClientSession::Create(SharedPtr<ServerSession> server_session, std::string name) {
    SharedPtr<ClientSession> client_session(new ClientSession);

    client_session->name = std::move(name);
    client_session->server_session = server_session;
    return MakeResult<SharedPtr<ClientSession>>(std::move(client_session));
}

ResultCode ClientSession::HandleSyncRequest() {
    // Signal the server session that new data is available
    return server_session->HandleSyncRequest();
}

} // namespace
