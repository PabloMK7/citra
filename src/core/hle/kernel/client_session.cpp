// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"

#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/server_session.h"

namespace Kernel {

ClientSession::ClientSession() = default;
ClientSession::~ClientSession() {
    // This destructor will be called automatically when the last ClientSession handle is closed by the emulated application.

    if (server_session->hle_handler)
        server_session->hle_handler->ClientDisconnected(server_session);

    // TODO(Subv): If the session is still open, set the connection status to 2 (Closed by client),
    // wake up all the ServerSession's waiting threads and set the WaitSynchronization result to 0xC920181A.
}

ResultVal<SharedPtr<ClientSession>> ClientSession::Create(ServerSession* server_session, std::string name) {
    SharedPtr<ClientSession> client_session(new ClientSession);

    client_session->name = std::move(name);
    client_session->server_session = server_session;
    client_session->session_status = SessionStatus::Open;
    return MakeResult<SharedPtr<ClientSession>>(std::move(client_session));
}

ResultCode ClientSession::SendSyncRequest() {
    // Signal the server session that new data is available
    return server_session->HandleSyncRequest();
}

} // namespace
