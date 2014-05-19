// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
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
class Interface  : public KernelObject {
    friend class Manager;
public:
    
    const char *GetName() { return GetPortName(); }
    const char *GetTypeName() { return GetPortName(); }

    static KernelIDType GetStaticIDType() { return KERNEL_ID_TYPE_THREAD; }
    KernelIDType GetIDType() const { return KERNEL_ID_TYPE_THREAD; }

    typedef void (*Function)(Interface*);

    struct FunctionInfo {
        u32         id;
        Function    func;
        std::string name;
    };

    /**
     * Gets the string name used by CTROS for a service
     * @return Port name of service
     */
    virtual const char *GetPortName() const {
        return "[UNKNOWN SERVICE PORT]";
    }

    /// Allocates a new handle for the service
    Handle NewHandle() {
        Handle handle = (m_handles.size() << 16) | 0;//m_handle;
        m_handles.push_back(handle);
        return handle;
    }

    /// Frees a handle from the service
    void DeleteHandle(Handle handle) {
        m_handles.erase(std::remove(m_handles.begin(), m_handles.end(), handle), m_handles.end());
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
                GetPortName(), cmd_buff[0]);
            return -1;
        }
        if (itr->second.func == NULL) {
            ERROR_LOG(OSHLE, "Unimplemented function: port = %s, name = %s!", 
                GetPortName(), itr->second.name.c_str());
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
    Interface* FetchFromHandle(u32 uid);

    /// Get a Service Interface from its port
    Interface* FetchFromPortName(std::string port_name);

private:

    std::vector<Interface*>     m_services;
    std::map<std::string, u32>  m_port_map;

};

/// Initialize ServiceManager
void Init();

/// Shutdown ServiceManager
void Shutdown();


extern Manager* g_manager; ///< Service manager


} // namespace
