// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common.h"
#include "common/string_util.h"

#include "core/hle/service/service.h"
#include "core/hle/service/ac_u.h"
#include "core/hle/service/am_app.h"
#include "core/hle/service/am_net.h"
#include "core/hle/service/apt_u.h"
#include "core/hle/service/boss_u.h"
#include "core/hle/service/cecd_u.h"
#include "core/hle/service/cfg_i.h"
#include "core/hle/service/cfg_u.h"
#include "core/hle/service/csnd_snd.h"
#include "core/hle/service/dsp_dsp.h"
#include "core/hle/service/err_f.h"
#include "core/hle/service/fs_user.h"
#include "core/hle/service/frd_u.h"
#include "core/hle/service/gsp_gpu.h"
#include "core/hle/service/hid_user.h"
#include "core/hle/service/ir_rst.h"
#include "core/hle/service/ir_u.h"
#include "core/hle/service/ldr_ro.h"
#include "core/hle/service/mic_u.h"
#include "core/hle/service/nim_aoc.h"
#include "core/hle/service/ndm_u.h"
#include "core/hle/service/nwm_uds.h"
#include "core/hle/service/pm_app.h"
#include "core/hle/service/ptm_u.h"
#include "core/hle/service/soc_u.h"
#include "core/hle/service/srv.h"
#include "core/hle/service/ssl_c.h"

namespace Service {

Manager* g_manager = nullptr;  ///< Service manager

////////////////////////////////////////////////////////////////////////////////////////////////////
// Service Manager class

Manager::Manager() {
}

Manager::~Manager() {
    for(Interface* service : m_services) {
        DeleteService(service->GetPortName());
    }
}

/// Add a service to the manager (does not create it though)
void Manager::AddService(Interface* service) {
    m_port_map[service->GetPortName()] = Kernel::g_object_pool.Create(service);
    m_services.push_back(service);
}

/// Removes a service from the manager, also frees memory
void Manager::DeleteService(const std::string& port_name) {
    Interface* service = FetchFromPortName(port_name);
    m_services.erase(std::remove(m_services.begin(), m_services.end(), service), m_services.end());
    m_port_map.erase(port_name);
    delete service;
}

/// Get a Service Interface from its Handle
Interface* Manager::FetchFromHandle(Handle handle) {
    return Kernel::g_object_pool.Get<Interface>(handle);
}

/// Get a Service Interface from its port
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
    g_manager->AddService(new AM_APP::Interface);
    g_manager->AddService(new AM_NET::Interface);
    g_manager->AddService(new APT_U::Interface);
    g_manager->AddService(new BOSS_U::Interface);
    g_manager->AddService(new CECD_U::Interface);
    g_manager->AddService(new CFG_I::Interface);
    g_manager->AddService(new CFG_U::Interface);
    g_manager->AddService(new CSND_SND::Interface);
    g_manager->AddService(new DSP_DSP::Interface);
    g_manager->AddService(new ERR_F::Interface);
    g_manager->AddService(new FRD_U::Interface);
    g_manager->AddService(new FS_User::Interface);
    g_manager->AddService(new GSP_GPU::Interface);
    g_manager->AddService(new HID_User::Interface);
    g_manager->AddService(new IR_RST::Interface);
    g_manager->AddService(new IR_U::Interface);
    g_manager->AddService(new LDR_RO::Interface);
    g_manager->AddService(new MIC_U::Interface);
    g_manager->AddService(new NIM_AOC::Interface);
    g_manager->AddService(new NDM_U::Interface);
    g_manager->AddService(new NWM_UDS::Interface);
    g_manager->AddService(new PM_APP::Interface);
    g_manager->AddService(new PTM_U::Interface);
    g_manager->AddService(new SOC_U::Interface);
    g_manager->AddService(new SSL_C::Interface);

    LOG_DEBUG(Service, "initialized OK");
}

/// Shutdown ServiceManager
void Shutdown() {
    delete g_manager;
    LOG_DEBUG(Service, "shutdown OK");
}


}
