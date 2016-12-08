// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/mvd/mvd.h"
#include "core/hle/service/mvd/mvd_std.h"
#include "core/hle/service/service.h"

namespace Service {
namespace MVD {

void Init() {
    AddService(new MVD_STD());
}

} // namespace MVD
} // namespace Service
