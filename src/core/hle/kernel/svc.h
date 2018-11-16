// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "common/common_types.h"

namespace Core {
class System;
} // namespace Core

namespace Kernel {

class SVC;

class SVCContext {
public:
    SVCContext(Core::System& system);
    ~SVCContext();
    void CallSVC(u32 immediate);

private:
    std::unique_ptr<SVC> impl;
};

} // namespace Kernel
