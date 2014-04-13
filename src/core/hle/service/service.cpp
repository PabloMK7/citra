// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common.h"
#include "common/log.h"

#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace Service

namespace Service {

Manager* g_manager = NULL;  ///< Service manager

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
    u32 new_uid = GetUIDFromIndex(index);

    m_services.push_back(service);

    m_port_map[service->GetPortName()] = new_uid;
    service->m_uid = new_uid;
}

/// Removes a service from the manager, also frees memory
void Manager::DeleteService(std::string port_name) {
    auto service = FetchFromPortName(port_name);

    m_services.erase(m_services.begin() + GetIndexFromUID(service->m_uid));
    m_port_map.erase(port_name);

    delete service;
}

/// Get a Service Interface from its UID
Interface* Manager::FetchFromUID(u32 uid) {
    int index = GetIndexFromUID(uid);
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
    return FetchFromUID(itr->second);
}

class Interface_SRV : public Interface {
public:

    Interface_SRV() {
    }

    ~Interface_SRV() {
    }

    /**
     * Gets the string name used by CTROS for a service
     * @return String name of service
     */
    std::string GetName() {
        return "ServiceManager";
    }

    /**
     * Gets the string name used by CTROS for a service
     * @return Port name of service
     */
    std::string GetPortName() {
        return "srv:";
    }

    /**
     * Called when svcSendSyncRequest is called, loads command buffer and executes comand
     * @return Return result of svcSendSyncRequest passed back to user app
     */
    Syscall::Result Sync() {
        ERROR_LOG(HLE, "Unimplemented function ServiceManager::Sync");
        return -1;
    }

};

/// Initialize ServiceManager
void Init() {
    g_manager = new Manager;
    g_manager->AddService(new Interface_SRV);
    NOTICE_LOG(HLE, "ServiceManager initialized OK");
}

/// Shutdown ServiceManager
void Shutdown() {
    delete g_manager;
    NOTICE_LOG(HLE, "ServiceManager shutdown OK");
}


}
