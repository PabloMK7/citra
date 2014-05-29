// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <vector>
#include <map>
#include <string>

#include "common/common.h"
#include "core/mem_map.h"

#include "core/hle/kernel/kernel.h"
#include "core/hle/svc.h"

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
class Interface  : public Kernel::Object {
    friend class Manager;
public:
    
    const char *GetName() { return GetPortName(); }
    const char *GetTypeName() { return GetPortName(); }

    static Kernel::HandleType GetStaticHandleType() { return Kernel::HandleType::Service; }
    Kernel::HandleType GetHandleType() const { return Kernel::HandleType::Service; }

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
    Handle CreateHandle(Kernel::Object *obj) {
        Handle handle = Kernel::g_object_pool.Create(obj);
        m_handles.push_back(handle);
        return handle;
    }

    /// Frees a handle from the service
    template <class T>
    void DeleteHandle(const Handle handle) {
        g_object_pool.Destroy<T>(handle);
        m_handles.erase(std::remove(m_handles.begin(), m_handles.end(), handle), m_handles.end());
    }

    /**
     * Synchronize kernel object 
     * @param wait Boolean wait set if current thread should wait as a result of sync operation
     * @return Result of operation, 0 on success, otherwise error code
     */
    Result SyncRequest(bool* wait) {
        u32* cmd_buff = GetCommandBuffer();
        auto itr = m_functions.find(cmd_buff[0]);

        if (itr == m_functions.end()) {
            ERROR_LOG(OSHLE, "Unknown/unimplemented function: port = %s, command = 0x%08X!", 
                GetPortName(), cmd_buff[0]);

            // TODO(bunnei): Hack - ignore error
            u32* cmd_buff = Service::GetCommandBuffer();
            cmd_buff[1] = 0;
            return 0; 
        }
        if (itr->second.func == NULL) {
            ERROR_LOG(OSHLE, "Unimplemented function: port = %s, name = %s!", 
                GetPortName(), itr->second.name.c_str());

            // TODO(bunnei): Hack - ignore error
            u32* cmd_buff = Service::GetCommandBuffer();
            cmd_buff[1] = 0;
            return 0; 
        } 

        itr->second.func(this);

        return 0; // TODO: Implement return from actual function
    }

    /**
     * Wait for kernel object to synchronize
     * @param wait Boolean wait set if current thread should wait as a result of sync operation
     * @return Result of operation, 0 on success, otherwise error code
     */
    Result WaitSynchronization(bool* wait) {
        // TODO(bunnei): ImplementMe
        return 0;
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

    std::vector<Handle>         m_handles;
    std::map<u32, FunctionInfo> m_functions;

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
