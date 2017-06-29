// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"

#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/kernel/session.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

ClientSession::ClientSession() = default;
ClientSession::~ClientSession() {
    // This destructor will be called automatically when the last ClientSession handle is closed by
    // the emulated application.

    // Local references to ServerSession and SessionRequestHandler are necessary to guarantee they
    // will be kept alive until after ClientDisconnected() returns.
    SharedPtr<ServerSession> server = parent->server;
    if (server) {
        std::shared_ptr<SessionRequestHandler> hle_handler = server->hle_handler;
        if (hle_handler)
            hle_handler->ClientDisconnected(server);

        // TODO(Subv): Force a wake up of all the ServerSession's waiting threads and set
        // their WaitSynchronization result to 0xC920181A.

        // Clean up the list of client threads with pending requests, they are unneeded now that the
        // client endpoint is closed.
        server->pending_requesting_threads.clear();
        server->currently_handling = nullptr;
    }

    parent->client = nullptr;
}

ResultCode ClientSession::SendSyncRequest(SharedPtr<Thread> thread) {
    // Keep ServerSession alive until we're done working with it.
    SharedPtr<ServerSession> server = parent->server;
    if (server == nullptr)
        return ERR_SESSION_CLOSED_BY_REMOTE;

    // Signal the server session that new data is available
    return server->HandleSyncRequest(std::move(thread));
}

} // namespace
