// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <map>
#include <string>

#include "common/common.h"
#include "common/common_types.h"
#include "core/mem_map.h"

#include "core/hle/kernel/kernel.h"
#include "core/hle/syscall.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace Service

namespace Service {

typedef s32 NativeUID;                          ///< Native handle for a service

static const int kMaxPortSize           = 0x08; ///< Maximum size of a port name (8 characters)
static const int kCommandHeaderOffset   = 0x80; ///< Offset into command buffer of header

/**
 * Returns a pointer to the command buffer in kernel memory
 * @param offset Optional offset into command buffer
 * @return Pointer to command buffer
 */
inline static u32* GetCommandBuffer(const int offset=0) {
    return (u32*)Memory::GetPointer(Memory::KERNEL_MEMORY_VADDR + kCommandHeaderOffset + offset);
}

class Manager;

/// Interface to a CTROS service
class Interface : NonCopyable {
    friend class Manager;
public:

    Interface() {
    }

    virtual ~Interface() {
    }

    typedef void (*Function)(Interface*);

    struct FunctionInfo {
        u32         id;
        Function    func;
        std::string name;
    };

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

    /// Allocates a new handle for the service
    Handle NewHandle() {
        Handle handle = (m_handles.size() << 16) | m_uid;
        m_handles.push_back(handle);
        return handle;
    }

    /// Frees a handle from the service
    void DeleteHandle(Handle handle) {
        for(auto iter = m_handles.begin(); iter != m_handles.end(); ++iter) {
            if(*iter == handle) {
                m_handles.erase(iter);
                break;
            }
        }
    }

    /**
     * Called when svcSendSyncRequest is called, loads command buffer and executes comand
     * @return Return result of svcSendSyncRequest passed back to user app
     */
    Result Sync() {
        u32* cmd_buff = GetCommandBuffer();
        auto itr = m_functions.find(cmd_buff[0]);

        if (itr == m_functions.end()) {
            ERROR_LOG(OSHLE, "Unknown/unimplemented function: port = %s, command = 0x%08X!", 
                GetPortName().c_str(), cmd_buff[0]);
            return -1;
        }
        if (itr->second.func == NULL) {
            ERROR_LOG(OSHLE, "Unimplemented function: port = %s, name = %s!", 
                GetPortName().c_str(), itr->second.name.c_str());
            return -1;
        } 

        itr->second.func(this);

        return 0; // TODO: Implement return from actual function
    }

protected:

    /**
     * Registers the functions in the service
     */
    void Register(const FunctionInfo* functions, int len) {
        for (int i = 0; i < len; i++) {
            m_functions[functions[i].id] = functions[i];
        }
    }

private:
    u32 m_uid;
    
    std::vector<Handle>    m_handles;
    std::map<u32, FunctionInfo>     m_functions;
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
};

/// Initialize ServiceManager
void Init();

/// Shutdown ServiceManager
void Shutdown();


extern Manager* g_manager; ///< Service manager


} // namespace
