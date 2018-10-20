// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <fmt/format.h>
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/ipc.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/server_port.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/service/ac/ac.h"
#include "core/hle/service/act/act.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/apt/apt.h"
#include "core/hle/service/boss/boss.h"
#include "core/hle/service/cam/cam.h"
#include "core/hle/service/cecd/cecd.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/csnd/csnd_snd.h"
#include "core/hle/service/dlp/dlp.h"
#include "core/hle/service/dsp/dsp_dsp.h"
#include "core/hle/service/err_f.h"
#include "core/hle/service/frd/frd.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/fs/fs_user.h"
#include "core/hle/service/gsp/gsp.h"
#include "core/hle/service/gsp/gsp_lcd.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/http_c.h"
#include "core/hle/service/ir/ir.h"
#include "core/hle/service/ldr_ro/ldr_ro.h"
#include "core/hle/service/mic_u.h"
#include "core/hle/service/mvd/mvd.h"
#include "core/hle/service/ndm/ndm_u.h"
#include "core/hle/service/news/news.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/hle/service/nim/nim.h"
#include "core/hle/service/ns/ns.h"
#include "core/hle/service/nwm/nwm.h"
#include "core/hle/service/pm/pm.h"
#include "core/hle/service/ps/ps_ps.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/hle/service/pxi/pxi.h"
#include "core/hle/service/qtm/qtm.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/sm/srv.h"
#include "core/hle/service/soc_u.h"
#include "core/hle/service/ssl_c.h"
#include "core/hle/service/y2r_u.h"

using Kernel::ClientPort;
using Kernel::ServerPort;
using Kernel::ServerSession;
using Kernel::SharedPtr;

namespace Service {

std::unordered_map<std::string, SharedPtr<ClientPort>> g_kernel_named_ports;

const std::array<ServiceModuleInfo, 40> service_module_map{
    {{"FS", 0x00040130'00001102, FS::InstallInterfaces},
     {"PM", 0x00040130'00001202, PM::InstallInterfaces},
     {"LDR", 0x00040130'00003702, LDR::InstallInterfaces},
     {"PXI", 0x00040130'00001402, PXI::InstallInterfaces},

     {"ERR", 0x00040030'00008A02, ERR::InstallInterfaces},
     {"AC", 0x00040130'00002402, AC::InstallInterfaces},
     {"ACT", 0x00040130'00003802, ACT::InstallInterfaces},
     {"AM", 0x00040130'00001502, AM::InstallInterfaces},
     {"BOSS", 0x00040130'00003402, BOSS::InstallInterfaces},
     {"CAM", 0x00040130'00001602,
      [](Core::System& system) {
          CAM::InstallInterfaces(system);
          Y2R::InstallInterfaces(system);
      }},
     {"CECD", 0x00040130'00002602, CECD::InstallInterfaces},
     {"CFG", 0x00040130'00001702, CFG::InstallInterfaces},
     {"DLP", 0x00040130'00002802, DLP::InstallInterfaces},
     {"DSP", 0x00040130'00001A02, DSP::InstallInterfaces},
     {"FRD", 0x00040130'00003202, FRD::InstallInterfaces},
     {"GSP", 0x00040130'00001C02, GSP::InstallInterfaces},
     {"HID", 0x00040130'00001D02, HID::InstallInterfaces},
     {"IR", 0x00040130'00003302, IR::InstallInterfaces},
     {"MIC", 0x00040130'00002002, MIC::InstallInterfaces},
     {"MVD", 0x00040130'20004102, MVD::InstallInterfaces},
     {"NDM", 0x00040130'00002B02, NDM::InstallInterfaces},
     {"NEWS", 0x00040130'00003502, NEWS::InstallInterfaces},
     {"NFC", 0x00040130'00004002, NFC::InstallInterfaces},
     {"NIM", 0x00040130'00002C02, NIM::InstallInterfaces},
     {"NS", 0x00040130'00008002,
      [](Core::System& system) {
          NS::InstallInterfaces(system);
          APT::InstallInterfaces(system);
      }},
     {"NWM", 0x00040130'00002D02, NWM::InstallInterfaces},
     {"PTM", 0x00040130'00002202, PTM::InstallInterfaces},
     {"QTM", 0x00040130'00004202, QTM::InstallInterfaces},
     {"CSND", 0x00040130'00002702, CSND::InstallInterfaces},
     {"HTTP", 0x00040130'00002902, HTTP::InstallInterfaces},
     {"SOC", 0x00040130'00002E02, SOC::InstallInterfaces},
     {"SSL", 0x00040130'00002F02, SSL::InstallInterfaces},
     // no HLE implementation
     {"CDC", 0x00040130'00001802, nullptr},
     {"GPIO", 0x00040130'00001B02, nullptr},
     {"I2C", 0x00040130'00001E02, nullptr},
     {"MCU", 0x00040130'00001F02, nullptr},
     {"MP", 0x00040130'00002A02, nullptr},
     {"PDN", 0x00040130'00002102, nullptr},
     {"PS", 0x00040130'00003102, nullptr},
     {"SPI", 0x00040130'00002302, nullptr}}};

/**
 * Creates a function string for logging, complete with the name (or header code, depending
 * on what's passed in) the port name, and all the cmd_buff arguments.
 */
static std::string MakeFunctionString(const char* name, const char* port_name,
                                      const u32* cmd_buff) {
    // Number of params == bits 0-5 + bits 6-11
    int num_params = (cmd_buff[0] & 0x3F) + ((cmd_buff[0] >> 6) & 0x3F);

    std::string function_string = fmt::format("function '{}': port={}", name, port_name);
    for (int i = 1; i <= num_params; ++i) {
        function_string += fmt::format(", cmd_buff[{}]={:#X}", i, cmd_buff[i]);
    }
    return function_string;
}

ServiceFrameworkBase::ServiceFrameworkBase(const char* service_name, u32 max_sessions,
                                           InvokerFn* handler_invoker)
    : service_name(service_name), max_sessions(max_sessions), handler_invoker(handler_invoker) {}

ServiceFrameworkBase::~ServiceFrameworkBase() = default;

void ServiceFrameworkBase::InstallAsService(SM::ServiceManager& service_manager) {
    ASSERT(port == nullptr);
    port = service_manager.RegisterService(service_name, max_sessions).Unwrap();
    port->SetHleHandler(shared_from_this());
}

void ServiceFrameworkBase::InstallAsNamedPort(Kernel::KernelSystem& kernel) {
    ASSERT(port == nullptr);
    SharedPtr<ServerPort> server_port;
    SharedPtr<ClientPort> client_port;
    std::tie(server_port, client_port) = kernel.CreatePortPair(max_sessions, service_name);
    server_port->SetHleHandler(shared_from_this());
    AddNamedPort(service_name, std::move(client_port));
}

void ServiceFrameworkBase::RegisterHandlersBase(const FunctionInfoBase* functions, std::size_t n) {
    handlers.reserve(handlers.size() + n);
    for (std::size_t i = 0; i < n; ++i) {
        // Usually this array is sorted by id already, so hint to insert at the end
        handlers.emplace_hint(handlers.cend(), functions[i].expected_header, functions[i]);
    }
}

void ServiceFrameworkBase::ReportUnimplementedFunction(u32* cmd_buf, const FunctionInfoBase* info) {
    IPC::Header header{cmd_buf[0]};
    int num_params = header.normal_params_size + header.translate_params_size;
    std::string function_name = info == nullptr ? fmt::format("{:#08x}", cmd_buf[0]) : info->name;

    fmt::memory_buffer buf;
    fmt::format_to(buf, "function '{}': port='{}' cmd_buf={{[0]={:#x}", function_name, service_name,
                   cmd_buf[0]);
    for (int i = 1; i <= num_params; ++i) {
        fmt::format_to(buf, ", [{}]={:#x}", i, cmd_buf[i]);
    }
    buf.push_back('}');

    LOG_ERROR(Service, "unknown / unimplemented {}", fmt::to_string(buf));
    // TODO(bunnei): Hack - ignore error
    cmd_buf[1] = 0;
}

void ServiceFrameworkBase::HandleSyncRequest(SharedPtr<ServerSession> server_session) {
    u32* cmd_buf = Kernel::GetCommandBuffer();

    u32 header_code = cmd_buf[0];
    auto itr = handlers.find(header_code);
    const FunctionInfoBase* info = itr == handlers.end() ? nullptr : &itr->second;
    if (info == nullptr || info->handler_callback == nullptr) {
        return ReportUnimplementedFunction(cmd_buf, info);
    }

    Kernel::SharedPtr<Kernel::Process> current_process =
        Core::System::GetInstance().Kernel().GetCurrentProcess();

    // TODO(yuriks): The kernel should be the one handling this as part of translation after
    // everything else is migrated
    Kernel::HLERequestContext context(std::move(server_session));
    context.PopulateFromIncomingCommandBuffer(cmd_buf, *current_process);

    LOG_TRACE(Service, "{}", MakeFunctionString(info->name, GetServiceName().c_str(), cmd_buf));
    handler_invoker(this, info->handler_callback, context);

    auto thread = Kernel::GetCurrentThread();
    ASSERT(thread->status == Kernel::ThreadStatus::Running ||
           thread->status == Kernel::ThreadStatus::WaitHleEvent);
    // Only write the response immediately if the thread is still running. If the HLE handler put
    // the thread to sleep then the writing of the command buffer will be deferred to the wakeup
    // callback.
    if (thread->status == Kernel::ThreadStatus::Running) {
        context.WriteToOutgoingCommandBuffer(cmd_buf, *current_process);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Module interface

// TODO(yuriks): Move to kernel
void AddNamedPort(std::string name, SharedPtr<ClientPort> port) {
    g_kernel_named_ports.emplace(std::move(name), std::move(port));
}

static bool AttemptLLE(const ServiceModuleInfo& service_module) {
    if (!Settings::values.lle_modules.at(service_module.name))
        return false;
    std::unique_ptr<Loader::AppLoader> loader =
        Loader::GetLoader(AM::GetTitleContentPath(FS::MediaType::NAND, service_module.title_id));
    if (!loader) {
        LOG_ERROR(Service,
                  "Service module \"{}\" could not be loaded; Defaulting to HLE implementation.",
                  service_module.name);
        return false;
    }
    SharedPtr<Kernel::Process> process;
    loader->Load(process);
    LOG_DEBUG(Service, "Service module \"{}\" has been successfully loaded.", service_module.name);
    return true;
}

/// Initialize ServiceManager
void Init(Core::System& core) {
    SM::ServiceManager::InstallInterfaces(core);

    for (const auto& service_module : service_module_map) {
        if (!AttemptLLE(service_module) && service_module.init_function != nullptr)
            service_module.init_function(core);
    }
    LOG_DEBUG(Service, "initialized OK");
}

/// Shutdown ServiceManager
void Shutdown() {

    g_kernel_named_ports.clear();
    LOG_DEBUG(Service, "shutdown OK");
}
} // namespace Service
