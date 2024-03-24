// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/hle/kernel/object.h"

namespace Kernel {

class ClientSession;
class ClientPort;
class ServerSession;

/**
 * Parent structure to link the client and server endpoints of a session with their associated
 * client port. The client port need not exist, as is the case for portless sessions like the
 * FS File and Directory sessions. When one of the endpoints of a session is destroyed, its
 * corresponding field in this structure will be set to nullptr.
 */
class Session final {
public:
    ClientSession* client = nullptr;  ///< The client endpoint of the session.
    ServerSession* server = nullptr;  ///< The server endpoint of the session.
    std::shared_ptr<ClientPort> port; ///< The port that this session is associated with (optional).
};
} // namespace Kernel
