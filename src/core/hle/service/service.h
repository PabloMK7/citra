// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "common/common_types.h"
#include "core/hle/syscall.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace Service

namespace Service {

typedef s32 NativeUID;

/// Interface to a CTROS service
class Interface {
public:

    virtual ~Interface() {
    }

    /**
     * Gets the UID for the serice
     * @return UID of service in native format
     */
    NativeUID GetUID() const {
        return (NativeUID)m_uid;
    }

    /**
     * Gets the string name used by CTROS for a service
     * @return String name of service
     */
    virtual std::string GetName() {
        return "[UNKNOWN SERVICE NAME]";
    }

    /**
     * Gets the string name used by CTROS for a service
     * @return Port name of service
     */
    virtual std::string GetPort() {
        return "[UNKNOWN SERVICE PORT]";
    }

    /**
     * Called when svcSendSyncRequest is called, loads command buffer and executes comand
     * @return Return result of svcSendSyncRequest passed back to user app
     */
    virtual Syscall::Result Sync() = 0;

private:
    u32 m_uid;
};

} // namespace
