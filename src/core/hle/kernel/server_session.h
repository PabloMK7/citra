// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "common/assert.h"
#include "common/common_types.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/result.h"
#include "core/hle/service/service.h"
#include "core/memory.h"

namespace Kernel {

class ClientSession;

/**
 * Kernel object representing the server endpoint of an IPC session. Sessions are the basic CTR-OS
 * primitive for communication between different processes, and are used to implement service calls
 * to the various system services.
 *
 * To make a service call, the client must write the command header and parameters to the buffer
 * located at offset 0x80 of the TLS (Thread-Local Storage) area, then execute a SendSyncRequest
 * SVC call with its ClientSession handle. The kernel will read the command header, using it to marshall
 * the parameters to the process at the server endpoint of the session. After the server replies to
 * the request, the response is marshalled back to the caller's TLS buffer and control is
 * transferred back to it.
 */
class ServerSession : public WaitObject {
public:
    ServerSession();
    ~ServerSession() override;

    /**
     * Creates a server session. The server session can have an optional HLE handler,
     * which will be invoked to handle the IPC requests that this session receives.
     * @param name Optional name of the server session.
     * @param hle_handler Optional HLE handler for this server session.
     * @return The created server session
     */
    static ResultVal<SharedPtr<ServerSession>> Create(std::string name = "Unknown", std::shared_ptr<Service::SessionRequestHandler> hle_handler = nullptr);

    std::string GetTypeName() const override {
        return "ServerSession";
    }

    static const HandleType HANDLE_TYPE = HandleType::ServerSession;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    /**
     * Creates a pair of ServerSession and an associated ClientSession.
     * @param name Optional name of the ports
     * @return The created session tuple
     */
    static std::tuple<SharedPtr<ServerSession>, SharedPtr<ClientSession>> CreateSessionPair(const std::string& name = "Unknown", std::shared_ptr<Service::SessionRequestHandler> hle_handler = nullptr);

    /**
     * Handle a sync request from the emulated application.
     * Only HLE services should override this function.
     * @returns ResultCode from the operation.
     */
    virtual ResultCode HandleSyncRequest();

    bool ShouldWait() override;

    void Acquire() override;

    std::string name; ///< The name of this session (optional)
    bool signaled;    ///< Whether there's new data available to this ServerSession
    std::shared_ptr<Service::SessionRequestHandler> hle_handler; ///< This session's HLE request handler (optional)
};
}
