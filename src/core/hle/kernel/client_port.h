// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <memory>
#include "common/common_types.h"
#include "core/hle/kernel/kernel.h"

namespace Service {
class Interface;
}

namespace Kernel {

class ServerPort;
class ServerSession;

class ClientPort final : public Object {
public:
    friend class ServerPort;

    /**
     * Creates a serverless ClientPort that represents a bridge between the HLE implementation of a service/port and the emulated application.
     * @param max_sessions Maximum number of sessions that this port is able to handle concurrently.
     * @param hle_interface Interface object that implements the commands of the service.
     * @returns ClientPort for the given HLE interface.
     */
    static Kernel::SharedPtr<ClientPort> CreateForHLE(u32 max_sessions, std::shared_ptr<Service::Interface> hle_interface);

    /**
     * Adds the specified server session to the queue of pending sessions of the associated ServerPort
     * @param server_session Server session to add to the queue
     */
    void AddWaitingSession(SharedPtr<ServerSession> server_session);

    std::string GetTypeName() const override { return "ClientPort"; }
    std::string GetName() const override { return name; }

    static const HandleType HANDLE_TYPE = HandleType::ClientPort;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    SharedPtr<ServerPort> server_port = nullptr;                 ///< ServerPort associated with this client port.
    u32 max_sessions;                                            ///< Maximum number of simultaneous sessions the port can have
    u32 active_sessions;                                         ///< Number of currently open sessions to this port
    std::string name;                                            ///< Name of client port (optional)
    std::shared_ptr<Service::Interface> hle_interface = nullptr; ///< HLE implementation of this port's request handler

private:
    ClientPort();
    ~ClientPort() override;
};

} // namespace
