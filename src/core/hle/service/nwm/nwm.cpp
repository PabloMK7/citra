// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/nwm/nwm.h"
#include "core/hle/service/nwm/nwm_cec.h"
#include "core/hle/service/nwm/nwm_ext.h"
#include "core/hle/service/nwm/nwm_inf.h"
#include "core/hle/service/nwm/nwm_sap.h"
#include "core/hle/service/nwm/nwm_soc.h"
#include "core/hle/service/nwm/nwm_tst.h"
#include "core/hle/service/nwm/nwm_uds.h"

namespace Service {
namespace NWM {

void Init() {
    AddService(new NWM_CEC);
    AddService(new NWM_EXT);
    AddService(new NWM_INF);
    AddService(new NWM_SAP);
    AddService(new NWM_SOC);
    AddService(new NWM_TST);
    AddService(new NWM_UDS);
}

} // namespace NWM
} // namespace Service
