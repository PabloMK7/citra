// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"

#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/server_port.h"
#include "core/hle/kernel/server_session.h"

namespace Kernel {

ClientPort::ClientPort(KernelSystem& kernel) : Object(kernel), kernel(kernel) {}
ClientPort::~ClientPort() = default;

Result ClientPort::Connect(std::shared_ptr<ClientSession>* out_client_session) {
    // Note: Threads do not wait for the server endpoint to call
    // AcceptSession before returning from this call.

    R_UNLESS(active_sessions < max_sessions, ResultMaxConnectionsReached);
    active_sessions++;

    // Create a new session pair, let the created sessions inherit the parent port's HLE handler.
    auto [server, client] = kernel.CreateSessionPair(server_port->GetName(), SharedFrom(this));

    if (server_port->hle_handler) {
        server_port->hle_handler->ClientConnected(server);
    } else {
        server_port->pending_sessions.push_back(server);
    }

    // Wake the threads waiting on the ServerPort
    server_port->WakeupAllWaitingThreads();

    *out_client_session = client;
    return ResultSuccess;
}

void ClientPort::ConnectionClosed() {
    ASSERT(active_sessions > 0);
    --active_sessions;
}

} // namespace Kernel
