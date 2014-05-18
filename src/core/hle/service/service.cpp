// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common.h"
#include "common/log.h"
#include "common/string_util.h"

#include "core/hle/hle.h"
#include "core/hle/service/service.h"
#include "core/hle/service/apt.h"
#include "core/hle/service/gsp.h"
#include "core/hle/service/hid.h"
#include "core/hle/service/srv.h"

namespace Service {

Manager* g_manager = NULL;  ///< Service manager

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
    int index = m_services.size();
    Handle handle = GetHandleFromIndex(index);

    m_services.push_back(service);

    m_port_map[service->GetPortName()] = handle;
    service->m_handle = handle;
}

/// Removes a service from the manager, also frees memory
void Manager::DeleteService(std::string port_name) {
    auto service = FetchFromPortName(port_name);

    m_services.erase(m_services.begin() + GetIndexFromHandle(service->m_handle));
    m_port_map.erase(port_name);

    delete service;
}

/// Get a Service Interface from its Handle
Interface* Manager::FetchFromHandle(Handle handle) {
    int index = GetIndexFromHandle(handle);
    if (index < (int)m_services.size()) {
        return m_services[index];
    }
    return NULL;
}

/// Get a Service Interface from its port
Interface* Manager::FetchFromPortName(std::string port_name) {
    auto itr = m_port_map.find(port_name);
    if (itr == m_port_map.end()) {
        return NULL;
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
    g_manager->AddService(new GSP_GPU::Interface);
    g_manager->AddService(new HID_User::Interface);

    NOTICE_LOG(HLE, "Services initialized OK");
}

/// Shutdown ServiceManager
void Shutdown() {
    delete g_manager;
    NOTICE_LOG(HLE, "Services shutdown OK");
}


}
