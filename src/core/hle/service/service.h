// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <map>
#include <string>

#include "common/common.h"
#include "common/common_types.h"
#include "core/hle/syscall.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace Service

namespace Service {

typedef s32 NativeUID;                          ///< Native handle for a service

static const int kCommandHeaderOffset = 0x80;   ///< Offset into command buffer of header

class Manager;

/// Interface to a CTROS service
class Interface {
    friend class Manager;
public:

    Interface() {
    }

    virtual ~Interface() {
    }

    /**
     * Gets the UID for the serice
     * @return UID of service in native format
     */
    NativeUID GetUID() const {
        return (NativeUID)m_uid;
    }

    /**
     * Gets the string name used by CTROS for a service
     * @return Port name of service
     */
    virtual std::string GetPortName() const {
        return "[UNKNOWN SERVICE PORT]";
    }

    /**
     * Called when svcSendSyncRequest is called, loads command buffer and executes comand
     * @return Return result of svcSendSyncRequest passed back to user app
     */
    Syscall::Result Sync() {
        u32* cmd_buff = (u32*)HLE::GetPointer(HLE::CMD_BUFFER_ADDR + kCommandHeaderOffset);
        auto itr = m_functions.find(cmd_buff[0]);

        if (itr == m_functions.end()) {
            ERROR_LOG(OSHLE, "Unknown/unimplemented function: port=%s, command=0x%08X!", 
                GetPortName().c_str(), cmd_buff[0]);
            return -1;
        }
        if (itr->second.func == NULL) {
            ERROR_LOG(OSHLE, "Unimplemented function: port=%s, name=%s!", 
                GetPortName().c_str(), itr->second.name.c_str());
        }

        itr->second.func();

        return 0; // TODO: Implement return from actual function
    }

protected:
    /**
     * Registers the functions in the service
     */
    void Register(const HLE::FunctionDef* functions, int len) {
        for (int i = 0; i < len; i++) {
            m_functions[functions[i].id] = functions[i];
        }
    }

private:
    u32 m_uid;
    std::map<u32, HLE::FunctionDef> m_functions;

    DISALLOW_COPY_AND_ASSIGN(Interface);
};

/// Simple class to manage accessing services from ports and UID handles
class Manager {

public:
    Manager();

    ~Manager();

    /// Add a service to the manager (does not create it though)
    void AddService(Interface* service);

    /// Removes a service from the manager (does not delete it though)
    void DeleteService(std::string port_name);

    /// Get a Service Interface from its UID
    Interface* FetchFromUID(u32 uid);

    /// Get a Service Interface from its port
    Interface* FetchFromPortName(std::string port_name);

private:

    /// Convert an index into m_services vector into a UID
    static u32 GetUIDFromIndex(const int index) {
        return index | 0x10000000;
    }

    /// Convert a UID into an index into m_services
    static int GetIndexFromUID(const u32 uid) {
        return uid & 0x0FFFFFFF;
    }

    std::vector<Interface*>     m_services;
    std::map<std::string, u32>  m_port_map;

    DISALLOW_COPY_AND_ASSIGN(Manager);
};

/// Initialize ServiceManager
void Init();

/// Shutdown ServiceManager
void Shutdown();


extern Manager* g_manager; ///< Service manager


} // namespace
