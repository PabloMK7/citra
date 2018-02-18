// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <random>
#include "core/hle/service/nwm/nwm.h"
#include "core/hle/service/nwm/nwm_cec.h"
#include "core/hle/service/nwm/nwm_ext.h"
#include "core/hle/service/nwm/nwm_inf.h"
#include "core/hle/service/nwm/nwm_sap.h"
#include "core/hle/service/nwm/nwm_soc.h"
#include "core/hle/service/nwm/nwm_tst.h"
#include "core/hle/service/nwm/nwm_uds.h"
#include "core/hle/shared_page.h"
#include "network/network.h"

namespace Service {
namespace NWM {

void Init() {
    AddService(new NWM_CEC);
    AddService(new NWM_EXT);
    AddService(new NWM_INF);
    AddService(new NWM_SAP);
    AddService(new NWM_SOC);
    AddService(new NWM_TST);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, std::numeric_limits<u8>::max());
    auto mac = SharedPage::DefaultMac;
    for (int i = 3; i < sizeof(SharedPage::MacAddress); ++i) {
        mac[i] = static_cast<u8>(dis(gen));
    }
    if (auto room_member = Network::GetRoomMember().lock()) {
        if (room_member->IsConnected()) {
            mac = static_cast<SharedPage::MacAddress>(room_member->GetMacAddress());
        }
    }
    SharedPage::SetMacAddress(mac);
    SharedPage::SetWifiLinkLevel(SharedPage::WifiLinkLevel::BEST);
}

void InstallInterfaces(SM::ServiceManager& service_manager) {
    std::make_shared<NWM_UDS>()->InstallAsService(service_manager);
}

} // namespace NWM
} // namespace Service
