// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/qtm/qtm.h"
#include "core/hle/service/qtm/qtm_s.h"
#include "core/hle/service/qtm/qtm_sp.h"
#include "core/hle/service/qtm/qtm_u.h"
#include "core/hle/service/service.h"

namespace Service {
namespace QTM {

void Init() {
    AddService(new QTM_S());
    AddService(new QTM_SP());
    AddService(new QTM_U());
}

} // namespace QTM
} // namespace Service
