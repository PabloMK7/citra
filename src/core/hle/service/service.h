// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/container/flat_map.hpp>

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

    /**
     * Creates a function string for logging, complete with the name (or header code, depending 
     * on what's passed in) the port name, and all the cmd_buff arguments.
     */
    std::string MakeFunctionString(const std::string& name, const std::string& port_name, const u32* cmd_buff) {
        // Number of params == bits 0-5 + bits 6-11
        int num_params = (cmd_buff[0] & 0x3F) + ((cmd_buff[0] >> 6) & 0x3F);

        std::string function_string = Common::StringFromFormat("function '%s': port=%s", name.c_str(), port_name.c_str());
        for (int i = 1; i <= num_params; ++i) {
            function_string += Common::StringFromFormat(", cmd_buff[%i]=%u", i, cmd_buff[i]);
        }
        return function_string;
    }

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

    ResultVal<bool> SyncRequest() override {
        u32* cmd_buff = Kernel::GetCommandBuffer();
        auto itr = m_functions.find(cmd_buff[0]);

        if (itr == m_functions.end() || itr->second.func == nullptr) {
            std::string function_name = (itr == m_functions.end()) ? Common::StringFromFormat("0x%08X", cmd_buff[0]) : itr->second.name;
            LOG_ERROR(Service, "%s %s", "unknown/unimplemented", MakeFunctionString(function_name, GetPortName(), cmd_buff).c_str());

            // TODO(bunnei): Hack - ignore error
            cmd_buff[1] = 0;
            return MakeResult<bool>(false);
        } else {
            LOG_TRACE(Service, "%s", MakeFunctionString(itr->second.name, GetPortName(), cmd_buff).c_str());
        }

        itr->second.func(this);

        return MakeResult<bool>(false); // TODO: Implement return from actual function
    }

protected:

    /**
     * Registers the functions in the service
     */
    template <size_t N>
    void Register(const FunctionInfo (&functions)[N]) {
        m_functions.reserve(N);
        for (auto& fn : functions) {
            // Usually this array is sorted by id already, so hint to instead at the end
            m_functions.emplace_hint(m_functions.cend(), fn.id, fn);
        }
    }

private:
    boost::container::flat_map<u32, FunctionInfo> m_functions;

};

/// Initialize ServiceManager
void Init();

/// Shutdown ServiceManager
void Shutdown();

/// Map of named ports managed by the kernel, which can be retrieved using the ConnectToPort SVC.
extern std::unordered_map<std::string, Kernel::SharedPtr<Interface>> g_kernel_named_ports;
/// Map of services registered with the "srv:" service, retrieved using GetServiceHandle.
extern std::unordered_map<std::string, Kernel::SharedPtr<Interface>> g_srv_services;

} // namespace
