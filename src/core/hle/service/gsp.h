// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace GSP_GPU

namespace GSP_GPU {

enum class GXCommandId : u32 {
    REQUEST_DMA            = 0x00000000,
    SET_COMMAND_LIST_LAST  = 0x00000001,
    SET_MEMORY_FILL        = 0x00000002, // TODO: Confirm? (lictru uses 0x01000102)
    SET_DISPLAY_TRANSFER   = 0x00000003,
    SET_TEXTURE_COPY       = 0x00000004,
    SET_COMMAND_LIST_FIRST = 0x00000005,
};

union GXCommand {
    struct {
        GXCommandId id;
    };

    u32 data[0x20];
};

/// Interface to "srv:" service
class Interface : public Service::Interface {
public:

    Interface();

    ~Interface();

    /**
     * Gets the string port name used by CTROS for the service
     * @return Port name of service
     */
    const char *GetPortName() const {
        return "gsp::Gpu";
    }

};

} // namespace
