// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/hle/service/service.h"

namespace Service::NEWS {

class NEWS_S final : public ServiceFramework<NEWS_S> {
public:
    NEWS_S();

private:
    /**
     * GetTotalNotifications service function.
     *  Inputs:
     *      0 : 0x00050000
     *  Outputs:
     *      0 : 0x00050080
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Number of notifications
     */
    void GetTotalNotifications(Kernel::HLERequestContext& ctx);

    /**
     * GetNewsDBHeader service function.
     *  Inputs:
     *      0 : 0x000A0042
     *      1 : Size
     *      2 : Output Buffer Mapping Translation Header ((Size << 4) | 0xC)
     *      3 : Output Buffer Pointer
     *  Outputs:
     *      0 : 0x000A0080
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Actual Size
     */
    void GetNewsDBHeader(Kernel::HLERequestContext& ctx);

    SERVICE_SERIALIZATION_SIMPLE
};

} // namespace Service::NEWS

BOOST_CLASS_EXPORT_KEY(Service::NEWS::NEWS_S)
