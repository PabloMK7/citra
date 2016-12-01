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

void ClientPort::AddWaitingSession(SharedPtr<ServerSession> server_session) {
    server_port->pending_sessions.push_back(server_session);
    // Wake the threads waiting on the ServerPort
    server_port->WakeupAllWaitingThreads();
}

} // namespace
