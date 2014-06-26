// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common.h"
#include "common/log.h"
#include "common/string_util.h"

#include "core/hle/hle.h"

#include "core/hle/service/service.h"
#include "core/hle/service/apt.h"
#include "core/hle/service/fs.h"
#include "core/hle/service/gsp.h"
#include "core/hle/service/hid.h"
#include "core/hle/service/ndm.h"
#include "core/hle/service/srv.h"

#include "core/hle/kernel/kernel.h"

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
void Manager::DeleteService(std::string port_name) {
    Interface* service = FetchFromPortName(port_name);
    m_services.erase(std::remove(m_services.begin(), m_services.end(), service), m_services.end());
    m_port_map.erase(port_name);
    delete service;
}

/// Get a Service Interface from its Handle
Interface* Manager::FetchFromHandle(Handle handle) {
    return Kernel::g_object_pool.GetFast<Interface>(handle);
}

/// Get a Service Interface from its port
Interface* Manager::FetchFromPortName(std::string port_name) {
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
    g_manager->AddService(new APT_U::Interface);
    g_manager->AddService(new FS_User::Interface);
    g_manager->AddService(new GSP_GPU::Interface);
    g_manager->AddService(new HID_User::Interface);
    g_manager->AddService(new NDM_U::Interface);

    NOTICE_LOG(HLE, "initialized OK");
}

/// Shutdown ServiceManager
void Shutdown() {
    delete g_manager;
    NOTICE_LOG(HLE, "shutdown OK");
}


}
