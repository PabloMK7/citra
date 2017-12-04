// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Kernel {
class HLERequestContext;
}

namespace Service {
namespace ERR {

/// Interface to "err:f" service
class ERR_F final : public ServiceFramework<ERR_F> {
public:
    ERR_F();
    ~ERR_F();

private:
    /* ThrowFatalError function
     * Inputs:
     *       0 : Header code [0x00010800]
     *    1-32 : FatalErrInfo
     * Outputs:
     *       0 : Header code
     *       1 : Result code
     */
    void ThrowFatalError(Kernel::HLERequestContext& ctx);
};

void InstallInterfaces();

} // namespace ERR
} // namespace Service
