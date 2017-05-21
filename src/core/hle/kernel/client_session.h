// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "common/common_types.h"

#include "core/hle/kernel/kernel.h"

namespace Kernel {

class ServerSession;
class Session;

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

    std::string name; ///< Name of client port (optional)

    /// The parent session, which links to the server endpoint.
    std::shared_ptr<Session> parent;

private:
    ClientSession();
    ~ClientSession() override;
};

} // namespace
