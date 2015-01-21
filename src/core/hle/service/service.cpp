// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common.h"
#include "common/string_util.h"

#include "core/hle/service/service.h"
#include "core/hle/service/ac_u.h"
#include "core/hle/service/act_u.h"
#include "core/hle/service/am_app.h"
#include "core/hle/service/am_net.h"
#include "core/hle/service/apt_a.h"
#include "core/hle/service/apt_s.h"
#include "core/hle/service/apt_u.h"
#include "core/hle/service/boss_u.h"
#include "core/hle/service/cecd_u.h"
#include "core/hle/service/cfg/cfg_i.h"
#include "core/hle/service/cfg/cfg_s.h"
#include "core/hle/service/cfg/cfg_u.h"
#include "core/hle/service/csnd_snd.h"
#include "core/hle/service/dsp_dsp.h"
#include "core/hle/service/err_f.h"
#include "core/hle/service/fs/fs_user.h"
#include "core/hle/service/frd_u.h"
#include "core/hle/service/gsp_gpu.h"
#include "core/hle/service/hid/hid_spvr.h"
#include "core/hle/service/hid/hid_user.h"
#include "core/hle/service/http_c.h"
#include "core/hle/service/ir_rst.h"
#include "core/hle/service/ir_u.h"
#include "core/hle/service/ldr_ro.h"
#include "core/hle/service/mic_u.h"
#include "core/hle/service/ndm_u.h"
#include "core/hle/service/news_u.h"
#include "core/hle/service/nim_aoc.h"
#include "core/hle/service/nwm_uds.h"
#include "core/hle/service/pm_app.h"
#include "core/hle/service/ptm_u.h"
#include "core/hle/service/ptm_sysm.h"
#include "core/hle/service/soc_u.h"
#include "core/hle/service/srv.h"
#include "core/hle/service/ssl_c.h"
#include "core/hle/service/y2r_u.h"

namespace Service {

Manager* g_manager = nullptr;  ///< Service manager

////////////////////////////////////////////////////////////////////////////////////////////////////
// Service Manager class

void Manager::AddService(Interface* service) {
    // TOOD(yuriks): Fix error reporting
    m_port_map[service->GetPortName()] = Kernel::g_handle_table.Create(service).ValueOr(INVALID_HANDLE);
    m_services.push_back(service);
}

void Manager::DeleteService(const std::string& port_name) {
    Interface* service = FetchFromPortName(port_name);
    m_services.erase(std::remove(m_services.begin(), m_services.end(), service), m_services.end());
    m_port_map.erase(port_name);
}

Interface* Manager::FetchFromHandle(Handle handle) {
    // TODO(yuriks): This function is very suspicious and should probably be exterminated.
    return Kernel::g_handle_table.Get<Interface>(handle).get();
}

Interface* Manager::FetchFromPortName(const std::string& port_name) {
    auto itr = m_port_map.find(port_name);
    if (itr == m_port_map.end()) {
        return nullptr;
    }
    return FetchFromHandle(itr->second);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Module interface

/// Initialize ServiceManager
void Init() {
    g_manager = new Manager;

    g_manager->AddService(new SRV::Interface);
    g_manager->AddService(new AC_U::Interface);
    g_manager->AddService(new ACT_U::Interface);
    g_manager->AddService(new AM_APP::Interface);
    g_manager->AddService(new AM_NET::Interface);
    g_manager->AddService(new APT_A::Interface);
    g_manager->AddService(new APT_S::Interface);
    g_manager->AddService(new APT_U::Interface);
    g_manager->AddService(new BOSS_U::Interface);
    g_manager->AddService(new CECD_U::Interface);
    g_manager->AddService(new CFG_I::Interface);
    g_manager->AddService(new CFG_S::Interface);
    g_manager->AddService(new CFG_U::Interface);
    g_manager->AddService(new CSND_SND::Interface);
    g_manager->AddService(new DSP_DSP::Interface);
    g_manager->AddService(new ERR_F::Interface);
    g_manager->AddService(new FRD_U::Interface);
    g_manager->AddService(new FS::FSUserInterface);
    g_manager->AddService(new GSP_GPU::Interface);
    g_manager->AddService(new HID_SPVR::Interface);
    g_manager->AddService(new HID_User::Interface);
    g_manager->AddService(new HTTP_C::Interface);
    g_manager->AddService(new IR_RST::Interface);
    g_manager->AddService(new IR_U::Interface);
    g_manager->AddService(new LDR_RO::Interface);
    g_manager->AddService(new MIC_U::Interface);
    g_manager->AddService(new NDM_U::Interface);
    g_manager->AddService(new NEWS_U::Interface);
    g_manager->AddService(new NIM_AOC::Interface);
    g_manager->AddService(new NWM_UDS::Interface);
    g_manager->AddService(new PM_APP::Interface);
    g_manager->AddService(new PTM_U::Interface);
    g_manager->AddService(new PTM_SYSM::Interface);
    g_manager->AddService(new SOC_U::Interface);
    g_manager->AddService(new SSL_C::Interface);
    g_manager->AddService(new Y2R_U::Interface);

    LOG_DEBUG(Service, "initialized OK");
}

/// Shutdown ServiceManager
void Shutdown() {
    delete g_manager;
    LOG_DEBUG(Service, "shutdown OK");
}


}
