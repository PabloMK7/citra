// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

#include <boost/container/flat_map.hpp>

#include "common/common_types.h"

#include "core/hle/kernel/session.h"
#include "core/hle/result.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace Service

namespace Service {

static const int kMaxPortSize = 8; ///< Maximum size of a port name (8 characters)

/// Interface to a CTROS service
class Interface : public Kernel::Session {
    // TODO(yuriks): An "Interface" being a Kernel::Object is mostly non-sense. Interface should be
    // just something that encapsulates a session and acts as a helper to implement service
    // processes.
public:
    std::string GetName() const override { return GetPortName(); }

    typedef void (*Function)(Interface*);

    struct FunctionInfo {
        u32         id;
        Function    func;
        const char* name;
    };

    /**
     * Gets the string name used by CTROS for a service
     * @return Port name of service
     */
    virtual std::string GetPortName() const {
        return "[UNKNOWN SERVICE PORT]";
    }

    ResultVal<bool> SyncRequest() override;

protected:

    /**
     * Registers the functions in the service
     */
    template <size_t N>
    inline void Register(const FunctionInfo (&functions)[N]) {
        Register(functions, N);
    }

    void Register(const FunctionInfo* functions, size_t n);

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

/// Adds a service to the services table
void AddService(Interface* interface_);

} // namespace
