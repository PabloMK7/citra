// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <vector>
#include <map>
#include <string>

#include "common/common.h"
#include "common/string_util.h"
#include "core/mem_map.h"

#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/session.h"
#include "core/hle/svc.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace Service

namespace Service {

static const int kMaxPortSize = 8; ///< Maximum size of a port name (8 characters)

class Manager;

/// Interface to a CTROS service
class Interface  : public Kernel::Session {
    // TODO(yuriks): An "Interface" being a Kernel::Object is mostly non-sense. Interface should be
    // just something that encapsulates a session and acts as a helper to implement service
    // processes.

    friend class Manager;
public:
    std::string GetName() const override { return GetPortName(); }

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
    virtual std::string GetPortName() const {
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
        Kernel::g_object_pool.Destroy<T>(handle);
        m_handles.erase(std::remove(m_handles.begin(), m_handles.end(), handle), m_handles.end());
    }

    ResultVal<bool> SyncRequest() override {
        u32* cmd_buff = Kernel::GetCommandBuffer();
        auto itr = m_functions.find(cmd_buff[0]);

        if (itr == m_functions.end() || itr->second.func == nullptr) {
            // Number of params == bits 0-5 + bits 6-11
            int num_params = (cmd_buff[0] & 0x3F) + ((cmd_buff[0] >> 6) & 0x3F);

            std::string error = "unknown/unimplemented function '%s': port=%s";
            for (int i = 1; i <= num_params; ++i) {
                error += Common::StringFromFormat(", cmd_buff[%i]=%u", i, cmd_buff[i]);
            }

            std::string name = (itr == m_functions.end()) ? Common::StringFromFormat("0x%08X", cmd_buff[0]) : itr->second.name;

            LOG_ERROR(Service, error.c_str(), name.c_str(), GetPortName().c_str());

            // TODO(bunnei): Hack - ignore error
            cmd_buff[1] = 0;
            return MakeResult<bool>(false);
        }

        itr->second.func(this);

        return MakeResult<bool>(false); // TODO: Implement return from actual function
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
    void DeleteService(const std::string& port_name);

    /// Get a Service Interface from its UID
    Interface* FetchFromHandle(u32 uid);

    /// Get a Service Interface from its port
    Interface* FetchFromPortName(const std::string& port_name);

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
