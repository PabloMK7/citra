// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <tuple>
#include "common/common_types.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/wait_object.h"

namespace Kernel {

class ClientPort;
class SessionRequestHandler;

class ServerPort final : public WaitObject {
public:
    /**
     * Creates a pair of ServerPort and an associated ClientPort.
     * @param max_sessions Maximum number of sessions to the port
     * @param name Optional name of the ports
     * @param hle_handler Optional HLE handler template for the port,
     * ServerSessions crated from this port will inherit a reference to this handler.
     * @return The created port tuple
     */
    static std::tuple<SharedPtr<ServerPort>, SharedPtr<ClientPort>> CreatePortPair(
        u32 max_sessions, std::string name = "UnknownPort",
        std::shared_ptr<SessionRequestHandler> hle_handler = nullptr);

    std::string GetTypeName() const override {
        return "ServerPort";
    }
    std::string GetName() const override {
        return name;
    }

    static const HandleType HANDLE_TYPE = HandleType::ServerPort;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    std::string name; ///< Name of port (optional)

    std::vector<SharedPtr<WaitObject>>
        pending_sessions; ///< ServerSessions waiting to be accepted by the port

    /// This session's HLE request handler template (optional)
    /// ServerSessions created from this port inherit a reference to this handler.
    std::shared_ptr<SessionRequestHandler> hle_handler;

    bool ShouldWait(Thread* thread) const override;
    void Acquire(Thread* thread) override;

private:
    ServerPort();
    ~ServerPort() override;
};

} // namespace
