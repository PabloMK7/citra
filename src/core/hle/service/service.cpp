// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/hle/service/ac_u.h"
#include "core/hle/service/act_a.h"
#include "core/hle/service/act_u.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/apt/apt.h"
#include "core/hle/service/boss/boss.h"
#include "core/hle/service/cam/cam.h"
#include "core/hle/service/cecd/cecd.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/csnd_snd.h"
#include "core/hle/service/dlp/dlp.h"
#include "core/hle/service/dsp_dsp.h"
#include "core/hle/service/err_f.h"
#include "core/hle/service/frd/frd.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/gsp_gpu.h"
#include "core/hle/service/gsp_lcd.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/http_c.h"
#include "core/hle/service/ir/ir.h"
#include "core/hle/service/ldr_ro/ldr_ro.h"
#include "core/hle/service/mic_u.h"
#include "core/hle/service/ndm/ndm.h"
#include "core/hle/service/news/news.h"
#include "core/hle/service/nim/nim.h"
#include "core/hle/service/ns_s.h"
#include "core/hle/service/nwm_uds.h"
#include "core/hle/service/pm_app.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/hle/service/service.h"
#include "core/hle/service/soc_u.h"
#include "core/hle/service/srv.h"
#include "core/hle/service/ssl_c.h"
#include "core/hle/service/y2r_u.h"

namespace Service {

std::unordered_map<std::string, Kernel::SharedPtr<Interface>> g_kernel_named_ports;
std::unordered_map<std::string, Kernel::SharedPtr<Interface>> g_srv_services;

/**
 * Creates a function string for logging, complete with the name (or header code, depending
 * on what's passed in) the port name, and all the cmd_buff arguments.
 */
static std::string MakeFunctionString(const char* name, const char* port_name,
                                      const u32* cmd_buff) {
    // Number of params == bits 0-5 + bits 6-11
    int num_params = (cmd_buff[0] & 0x3F) + ((cmd_buff[0] >> 6) & 0x3F);

    std::string function_string =
        Common::StringFromFormat("function '%s': port=%s", name, port_name);
    for (int i = 1; i <= num_params; ++i) {
        function_string += Common::StringFromFormat(", cmd_buff[%i]=0x%X", i, cmd_buff[i]);
    }
    return function_string;
}

ResultVal<bool> Interface::SyncRequest() {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    auto itr = m_functions.find(cmd_buff[0]);

    if (itr == m_functions.end() || itr->second.func == nullptr) {
        std::string function_name = (itr == m_functions.end())
                                        ? Common::StringFromFormat("0x%08X", cmd_buff[0])
                                        : itr->second.name;
        LOG_ERROR(
            Service, "unknown / unimplemented %s",
            MakeFunctionString(function_name.c_str(), GetPortName().c_str(), cmd_buff).c_str());

        // TODO(bunnei): Hack - ignore error
        cmd_buff[1] = 0;
        return MakeResult<bool>(false);
    }
    LOG_TRACE(Service, "%s",
              MakeFunctionString(itr->second.name, GetPortName().c_str(), cmd_buff).c_str());

    itr->second.func(this);

    return MakeResult<bool>(false); // TODO: Implement return from actual function
}

void Interface::Register(const FunctionInfo* functions, size_t n) {
    m_functions.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        // Usually this array is sorted by id already, so hint to instead at the end
        m_functions.emplace_hint(m_functions.cend(), functions[i].id, functions[i]);
    }
}

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
    Service::AM::Init();
    Service::APT::Init();
    Service::BOSS::Init();
    Service::CAM::Init();
    Service::CECD::Init();
    Service::CFG::Init();
    Service::DLP::Init();
    Service::FRD::Init();
    Service::HID::Init();
    Service::IR::Init();
    Service::NEWS::Init();
    Service::NDM::Init();
    Service::NIM::Init();
    Service::PTM::Init();

    AddService(new AC_U::Interface);
    AddService(new ACT_A::Interface);
    AddService(new ACT_U::Interface);
    AddService(new CSND_SND::Interface);
    AddService(new DSP_DSP::Interface);
    AddService(new GSP_GPU::Interface);
    AddService(new GSP_LCD::Interface);
    AddService(new HTTP_C::Interface);
    AddService(new LDR_RO::Interface);
    AddService(new MIC_U::Interface);
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

    Service::PTM::Shutdown();
    Service::NDM::Shutdown();
    Service::NIM::Shutdown();
    Service::NEWS::Shutdown();
    Service::IR::Shutdown();
    Service::HID::Shutdown();
    Service::FRD::Shutdown();
    Service::DLP::Shutdown();
    Service::CFG::Shutdown();
    Service::CECD::Shutdown();
    Service::CAM::Shutdown();
    Service::BOSS::Shutdown();
    Service::APT::Shutdown();
    Service::AM::Shutdown();
    Service::FS::ArchiveShutdown();

    g_srv_services.clear();
    g_kernel_named_ports.clear();
    LOG_DEBUG(Service, "shutdown OK");
}
}
