// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <memory>

#include "common/common_types.h"

#include "core/hle/kernel/kernel.h"

namespace Kernel {

class ServerSession;

class ClientSession final : public Object {
public:
    friend class ServerSession;

    std::string GetTypeName() const override {
        return "ClientSession";
    }

    std::string GetName() const override {
        return name;
    }

    static const HandleType HANDLE_TYPE = HandleType::ClientSession;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    /**
     * Sends an SyncRequest from the current emulated thread.
     * @return ResultCode of the operation.
     */
    ResultCode SendSyncRequest();

    std::string name;                           ///< Name of client port (optional)
    SharedPtr<ServerSession> server_session;    ///< The server session associated with this client session.

private:
    ClientSession();
    ~ClientSession() override;

    /**
     * Creates a client session.
     * @param server_session The server session associated with this client session
     * @param name Optional name of client session
     * @return The created client session
     */
    static ResultVal<SharedPtr<ClientSession>> Create(SharedPtr<ServerSession> server_session, std::string name = "Unknown");
};

} // namespace
