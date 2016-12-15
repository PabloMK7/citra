// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/act/act.h"
#include "core/hle/service/act/act_a.h"
#include "core/hle/service/act/act_u.h"

namespace Service {
namespace ACT {

void Init() {
    AddService(new ACT_A);
    AddService(new ACT_U);
}

} // namespace ACT
} // namespace Service
