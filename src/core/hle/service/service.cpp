// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <fmt/format.h>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/hle/ipc.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/event.h"
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
#include "core/hle/service/csnd_snd.h"
#include "core/hle/service/dlp/dlp.h"
#include "core/hle/service/dsp_dsp.h"
#include "core/hle/service/err_f.h"
#include "core/hle/service/frd/frd.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/gsp/gsp.h"
#include "core/hle/service/gsp_lcd.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/http_c.h"
#include "core/hle/service/ir/ir.h"
#include "core/hle/service/ldr_ro/ldr_ro.h"
#include "core/hle/service/mic_u.h"
#include "core/hle/service/mvd/mvd.h"
#include "core/hle/service/ndm/ndm.h"
#include "core/hle/service/news/news.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/hle/service/nim/nim.h"
#include "core/hle/service/ns/ns.h"
#include "core/hle/service/nwm/nwm.h"
#include "core/hle/service/pm_app.h"
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

Interface::Interface(u32 max_sessions) : max_sessions(max_sessions) {}
Interface::~Interface() = default;

void Interface::HandleSyncRequest(SharedPtr<ServerSession> server_session) {
    // TODO(Subv): Make use of the server_session in the HLE service handlers to distinguish which
    // session triggered each command.

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
        return;
    }
    LOG_TRACE(Service, "%s",
              MakeFunctionString(itr->second.name, GetPortName().c_str(), cmd_buff).c_str());

    itr->second.func(this);
}

void Interface::Register(const FunctionInfo* functions, size_t n) {
    m_functions.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        // Usually this array is sorted by id already, so hint to instead at the end
        m_functions.emplace_hint(m_functions.cend(), functions[i].id, functions[i]);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

ServiceFrameworkBase::ServiceFrameworkBase(const char* service_name, u32 max_sessions,
                                           InvokerFn* handler_invoker)
    : service_name(service_name), max_sessions(max_sessions), handler_invoker(handler_invoker) {}

ServiceFrameworkBase::~ServiceFrameworkBase() = default;

void ServiceFrameworkBase::InstallAsService(SM::ServiceManager& service_manager) {
    ASSERT(port == nullptr);
    port = service_manager.RegisterService(service_name, max_sessions).Unwrap();
    port->SetHleHandler(shared_from_this());
}

void ServiceFrameworkBase::InstallAsNamedPort() {
    ASSERT(port == nullptr);
    SharedPtr<ServerPort> server_port;
    SharedPtr<ClientPort> client_port;
    std::tie(server_port, client_port) = ServerPort::CreatePortPair(max_sessions, service_name);
    server_port->SetHleHandler(shared_from_this());
    AddNamedPort(service_name, std::move(client_port));
}

void ServiceFrameworkBase::RegisterHandlersBase(const FunctionInfoBase* functions, size_t n) {
    handlers.reserve(handlers.size() + n);
    for (size_t i = 0; i < n; ++i) {
        // Usually this array is sorted by id already, so hint to insert at the end
        handlers.emplace_hint(handlers.cend(), functions[i].expected_header, functions[i]);
    }
}

void ServiceFrameworkBase::ReportUnimplementedFunction(u32* cmd_buf, const FunctionInfoBase* info) {
    IPC::Header header{cmd_buf[0]};
    int num_params = header.normal_params_size + header.translate_params_size;
    std::string function_name = info == nullptr ? fmt::format("{:#08x}", cmd_buf[0]) : info->name;

    fmt::MemoryWriter w;
    w.write("function '{}': port='{}' cmd_buf={{[0]={:#x}", function_name, service_name,
            cmd_buf[0]);
    for (int i = 1; i <= num_params; ++i) {
        w.write(", [{}]={:#x}", i, cmd_buf[i]);
    }
    w << '}';

    LOG_ERROR(Service, "unknown / unimplemented %s", w.c_str());
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

    // TODO(yuriks): The kernel should be the one handling this as part of translation after
    // everything else is migrated
    Kernel::HLERequestContext context(std::move(server_session));
    context.PopulateFromIncomingCommandBuffer(cmd_buf, *Kernel::g_current_process,
                                              Kernel::g_handle_table);

    LOG_TRACE(Service, "%s",
              MakeFunctionString(info->name, GetServiceName().c_str(), cmd_buf).c_str());
    handler_invoker(this, info->handler_callback, context);
    context.WriteToOutgoingCommandBuffer(cmd_buf, *Kernel::g_current_process,
                                         Kernel::g_handle_table);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Module interface

// TODO(yuriks): Move to kernel
void AddNamedPort(std::string name, SharedPtr<ClientPort> port) {
    g_kernel_named_ports.emplace(std::move(name), std::move(port));
}

static void AddNamedPort(Interface* interface_) {
    SharedPtr<ServerPort> server_port;
    SharedPtr<ClientPort> client_port;
    std::tie(server_port, client_port) =
        ServerPort::CreatePortPair(interface_->GetMaxSessions(), interface_->GetPortName());

    server_port->SetHleHandler(std::shared_ptr<Interface>(interface_));
    AddNamedPort(interface_->GetPortName(), std::move(client_port));
}

void AddService(Interface* interface_) {
    auto server_port =
        SM::g_service_manager
            ->RegisterService(interface_->GetPortName(), interface_->GetMaxSessions())
            .Unwrap();
    server_port->SetHleHandler(std::shared_ptr<Interface>(interface_));
}

bool ThreadContinuationToken::IsValid() {
    return thread != nullptr && event != nullptr;
}

ThreadContinuationToken SleepClientThread(const std::string& reason,
                                          ThreadContinuationToken::Callback callback) {
    auto thread = Kernel::GetCurrentThread();

    ASSERT(thread->status == THREADSTATUS_RUNNING);

    ThreadContinuationToken token;

    token.event = Kernel::Event::Create(Kernel::ResetType::OneShot, "HLE Pause Event: " + reason);
    token.thread = thread;
    token.callback = std::move(callback);
    token.pause_reason = std::move(reason);

    // Make the thread wait on our newly created event, it will be signaled when
    // ContinueClientThread is called.
    thread->status = THREADSTATUS_WAIT_HLE_EVENT;
    thread->wait_objects = {token.event};
    token.event->AddWaitingThread(thread);

    return token;
}

void ContinueClientThread(ThreadContinuationToken& token) {
    ASSERT_MSG(token.IsValid(), "Invalid continuation token");
    ASSERT(token.thread->status == THREADSTATUS_WAIT_HLE_EVENT);

    // Signal the event to wake up the thread
    token.event->Signal();
    ASSERT(token.thread->status == THREADSTATUS_READY);

    token.callback(token.thread);

    token.event = nullptr;
    token.thread = nullptr;
    token.callback = nullptr;
}

/// Initialize ServiceManager
void Init() {
    SM::g_service_manager = std::make_shared<SM::ServiceManager>();
    SM::ServiceManager::InstallInterfaces(SM::g_service_manager);

    ERR::InstallInterfaces();

    PXI::InstallInterfaces(*SM::g_service_manager);
    NS::InstallInterfaces(*SM::g_service_manager);
    AC::InstallInterfaces(*SM::g_service_manager);
    LDR::InstallInterfaces(*SM::g_service_manager);
    MIC::InstallInterfaces(*SM::g_service_manager);

    FS::ArchiveInit();
    ACT::Init();
    AM::Init();
    APT::Init();
    BOSS::Init();
    CAM::InstallInterfaces(*SM::g_service_manager);
    CECD::Init();
    CFG::Init();
    DLP::Init();
    FRD::Init();
    GSP::InstallInterfaces(*SM::g_service_manager);
    HID::InstallInterfaces(*SM::g_service_manager);
    IR::InstallInterfaces(*SM::g_service_manager);
    MVD::Init();
    NDM::Init();
    NEWS::Init();
    NFC::Init();
    NIM::Init();
    NWM::Init();
    PTM::InstallInterfaces(*SM::g_service_manager);
    QTM::Init();

    AddService(new CSND::CSND_SND);
    AddService(new DSP_DSP::Interface);
    AddService(new GSP::GSP_LCD);
    AddService(new HTTP::HTTP_C);
    AddService(new PM::PM_APP);
    AddService(new SOC::SOC_U);
    AddService(new SSL::SSL_C);
    Y2R::InstallInterfaces(*SM::g_service_manager);

    LOG_DEBUG(Service, "initialized OK");
}

/// Shutdown ServiceManager
void Shutdown() {
    NFC::Shutdown();
    NIM::Shutdown();
    NEWS::Shutdown();
    NDM::Shutdown();
    FRD::Shutdown();
    DLP::Shutdown();
    CFG::Shutdown();
    CECD::Shutdown();
    BOSS::Shutdown();
    APT::Shutdown();
    AM::Shutdown();
    FS::ArchiveShutdown();

    SM::g_service_manager = nullptr;
    g_kernel_named_ports.clear();
    LOG_DEBUG(Service, "shutdown OK");
}
} // namespace Service
