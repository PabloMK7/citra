// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace SRV

namespace SRV {

/// Interface to "srv:" service
class Interface : public Service::Interface {

public:

    Interface();

    ~Interface();

    /**
     * Gets the string name used by CTROS for the service
     * @return Port name of service
     */
    std::string GetPortName() const {
        return "srv:";
    }

    /**
     * Called when svcSendSyncRequest is called, loads command buffer and executes comand
     * @return Return result of svcSendSyncRequest passed back to user app
     */
    Result Sync();

};

} // namespace
