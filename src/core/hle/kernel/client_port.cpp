// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/server_port.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/service/service.h"

namespace Kernel {

ClientPort::ClientPort() {}
ClientPort::~ClientPort() {}

Kernel::SharedPtr<ClientPort> ClientPort::CreateForHLE(u32 max_sessions, std::shared_ptr<Service::Interface> hle_interface) {
    SharedPtr<ClientPort> client_port(new ClientPort);
    client_port->max_sessions = max_sessions;
    client_port->active_sessions = 0;
    client_port->name = hle_interface->GetPortName();
    client_port->hle_interface = std::move(hle_interface);

    return client_port;
}

void ClientPort::AddWaitingSession(SharedPtr<ServerSession> server_session) {
    // A port that has an associated HLE interface doesn't have a server port.
    if (hle_interface != nullptr)
        return;

    server_port->pending_sessions.push_back(server_session);
    // Wake the threads waiting on the ServerPort
    server_port->WakeupAllWaitingThreads();
}

} // namespace
