// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common.h"
#include "common/log.h"

#include "core/hle/hle.h"
#include "core/hle/service/service.h"
#include "core/hle/service/apt.h"

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

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface to "SRV" service

class SRV : public Interface {

public:

    SRV() {
    }

    ~SRV() {
    }

    enum {
        CMD_OFFSET              = 0x80,
        CMD_HEADER_INIT         = 0x10002,  ///< Command header to initialize SRV service
        CMD_HEADER_GET_HANDLE   = 0x50100,  ///< Command header to get handle of other services
    };

    /**
     * Gets the string name used by CTROS for a service
     * @return String name of service
     */
    std::string GetName() const {
        return "ServiceManager";
    }

    /**
     * Gets the string name used by CTROS for a service
     * @return Port name of service
     */
    std::string GetPortName() const {
        return "srv:";
    }

    /**
     * Called when svcSendSyncRequest is called, loads command buffer and executes comand
     * @return Return result of svcSendSyncRequest passed back to user app
     */
    Syscall::Result Sync() {
        u32 header = 0;
        Syscall::Result res = 0;
        
        u32* cmd_buff = (u32*)HLE::GetPointer(HLE::CMD_BUFFER_ADDR + CMD_OFFSET);

        switch (cmd_buff[0]) {

        case CMD_HEADER_INIT:
            NOTICE_LOG(OSHLE, "SRV::Sync - Initialize");
            break;

        case CMD_HEADER_GET_HANDLE:
            const char* port_name = (const char*)&cmd_buff[1];
            Interface* service = g_manager->FetchFromPortName(port_name);

            NOTICE_LOG(OSHLE, "SRV::Sync - GetHandle - port: %s, handle: 0x%08X", port_name, 
                service->GetUID());

            if (NULL != service) {
                cmd_buff[3] = service->GetUID();
            } else {
                ERROR_LOG(OSHLE, "Service %s does not exist", port_name);
                res = -1;
            }
            
            break;
        }

        cmd_buff[1] = res;

        return res;
    }
     
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Module interface

/// Initialize ServiceManager
void Init() {
    g_manager = new Manager;
    g_manager->AddService(new SRV);
    g_manager->AddService(new APT);
    NOTICE_LOG(HLE, "Services initialized OK");
}

/// Shutdown ServiceManager
void Shutdown() {
    delete g_manager;
    NOTICE_LOG(HLE, "Services shutdown OK");
}


}
