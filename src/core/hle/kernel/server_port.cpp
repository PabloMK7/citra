// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>
#include "common/assert.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/server_port.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

ServerPort::ServerPort(KernelSystem& kernel) : WaitObject(kernel) {}
ServerPort::~ServerPort() {}

ResultVal<SharedPtr<ServerSession>> ServerPort::Accept() {
    if (pending_sessions.empty()) {
        return ERR_NO_PENDING_SESSIONS;
    }

    auto session = std::move(pending_sessions.back());
    pending_sessions.pop_back();
    return MakeResult(std::move(session));
}

bool ServerPort::ShouldWait(Thread* thread) const {
    // If there are no pending sessions, we wait until a new one is added.
    return pending_sessions.size() == 0;
}

void ServerPort::Acquire(Thread* thread) {
    ASSERT_MSG(!ShouldWait(thread), "object unavailable!");
}

std::tuple<SharedPtr<ServerPort>, SharedPtr<ClientPort>> KernelSystem::CreatePortPair(
    u32 max_sessions, std::string name) {

    SharedPtr<ServerPort> server_port(new ServerPort(*this));
    SharedPtr<ClientPort> client_port(new ClientPort(*this));

    server_port->name = name + "_Server";
    client_port->name = name + "_Client";
    client_port->server_port = server_port;
    client_port->max_sessions = max_sessions;
    client_port->active_sessions = 0;

    return std::make_tuple(std::move(server_port), std::move(client_port));
}

} // namespace Kernel
