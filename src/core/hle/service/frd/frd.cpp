// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/service.h"
#include "core/hle/service/frd/frd.h"
#include "core/hle/service/frd/frd_a.h"
#include "core/hle/service/frd/frd_u.h"

namespace Service {
namespace FRD {

void Init() {
    using namespace Kernel;

    AddService(new FRD_A_Interface);
    AddService(new FRD_U_Interface);
}

void Shutdown() {
}

} // namespace FRD

} // namespace Service
