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
#include "core/hle/service/am_sys.h"
#include "core/hle/service/boss_p.h"
#include "core/hle/service/boss_u.h"
#include "core/hle/service/cam_u.h"
#include "core/hle/service/cecd_u.h"
#include "core/hle/service/cecd_s.h"
#include "core/hle/service/csnd_snd.h"
#include "core/hle/service/dsp_dsp.h"
#include "core/hle/service/err_f.h"
#include "core/hle/service/frd_a.h"
#include "core/hle/service/frd_u.h"
#include "core/hle/service/gsp_gpu.h"
#include "core/hle/service/gsp_lcd.h"
#include "core/hle/service/http_c.h"
#include "core/hle/service/ir_rst.h"
#include "core/hle/service/ir_u.h"
#include "core/hle/service/ldr_ro.h"
#include "core/hle/service/mic_u.h"
#include "core/hle/service/ndm_u.h"
#include "core/hle/service/news_s.h"
#include "core/hle/service/news_u.h"
#include "core/hle/service/nim_aoc.h"
#include "core/hle/service/ns_s.h"
#include "core/hle/service/nwm_uds.h"
#include "core/hle/service/pm_app.h"
#include "core/hle/service/soc_u.h"
#include "core/hle/service/srv.h"
#include "core/hle/service/ssl_c.h"
#include "core/hle/service/y2r_u.h"

#include "core/hle/service/apt/apt.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/ptm/ptm.h"

namespace Service {

std::unordered_map<std::string, Kernel::SharedPtr<Interface>> g_kernel_named_ports;
std::unordered_map<std::string, Kernel::SharedPtr<Interface>> g_srv_services;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Module interface

static void AddNamedPort(Interface* interface_) {
    g_kernel_named_ports.emplace(interface_->GetPortName(), interface_);
}

void AddService(Interface* interface_) {
    g_srv_services.emplace(interface_->GetPortName(), interface_);
}

/// Initialize ServiceManager
void Init() {
    AddNamedPort(new SRV::Interface);
    AddNamedPort(new ERR_F::Interface);

    Service::FS::ArchiveInit();
    Service::CFG::CFGInit();
    Service::APT::APTInit();
    Service::PTM::PTMInit();
    Service::HID::HIDInit();

    AddService(new AC_U::Interface);
    AddService(new ACT_U::Interface);
    AddService(new AM_APP::Interface);
    AddService(new AM_NET::Interface);
    AddService(new AM_SYS::Interface);
    AddService(new BOSS_P::Interface);
    AddService(new BOSS_U::Interface);
    AddService(new CAM_U::Interface);
    AddService(new CECD_S::Interface);
    AddService(new CECD_U::Interface);
    AddService(new CSND_SND::Interface);
    AddService(new DSP_DSP::Interface);
    AddService(new FRD_A::Interface);
    AddService(new FRD_U::Interface);
    AddService(new GSP_GPU::Interface);
    AddService(new GSP_LCD::Interface);
    AddService(new HTTP_C::Interface);
    AddService(new IR_RST::Interface);
    AddService(new IR_U::Interface);
    AddService(new LDR_RO::Interface);
    AddService(new MIC_U::Interface);
    AddService(new NDM_U::Interface);
    AddService(new NEWS_S::Interface);
    AddService(new NEWS_U::Interface);
    AddService(new NIM_AOC::Interface);
    AddService(new NS_S::Interface);
    AddService(new NWM_UDS::Interface);
    AddService(new PM_APP::Interface);
    AddService(new SOC_U::Interface);
    AddService(new SSL_C::Interface);
    AddService(new Y2R_U::Interface);

    LOG_DEBUG(Service, "initialized OK");
}

/// Shutdown ServiceManager
void Shutdown() {
    Service::HID::HIDShutdown();
    Service::PTM::PTMShutdown();
    Service::APT::APTShutdown();
    Service::CFG::CFGShutdown();
    Service::FS::ArchiveShutdown();

    g_srv_services.clear();
    g_kernel_named_ports.clear();
    LOG_DEBUG(Service, "shutdown OK");
}


}
