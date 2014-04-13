// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <map>
#include <string>

#include "common/common_types.h"
#include "core/hle/syscall.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace Service

namespace Service {

typedef s32 NativeUID;          ///< Native handle for a service

class Manager;

/// Interface to a CTROS service
class Interface {
    friend class Manager;
public:

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
     * @return String name of service
     */
    virtual std::string GetName() {
        return "[UNKNOWN SERVICE NAME]";
    }

    /**
     * Gets the string name used by CTROS for a service
     * @return Port name of service
     */
    virtual std::string GetPortName() {
        return "[UNKNOWN SERVICE PORT]";
    }

    /**
     * Called when svcSendSyncRequest is called, loads command buffer and executes comand
     * @return Return result of svcSendSyncRequest passed back to user app
     */
    virtual Syscall::Result Sync() = 0;

private:
    u32 m_uid;
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
