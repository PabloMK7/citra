// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"

#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/server_session.h"

namespace Kernel {

ClientSession::ClientSession() = default;
ClientSession::~ClientSession() {
    // This destructor will be called automatically when the last ClientSession handle is closed by
    // the emulated application.

    if (parent->server) {
        if (parent->server->hle_handler)
            parent->server->hle_handler->ClientDisconnected(parent->server);

        // TODO(Subv): Force a wake up of all the ServerSession's waiting threads and set
        // their WaitSynchronization result to 0xC920181A.
    }

    parent->client = nullptr;
}

ResultCode ClientSession::SendSyncRequest() {
    // Signal the server session that new data is available
    if (parent->server)
        return parent->server->HandleSyncRequest();

    return ERR_SESSION_CLOSED_BY_REMOTE;
}

} // namespace
