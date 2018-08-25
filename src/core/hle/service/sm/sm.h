// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "core/hle/kernel/object.h"
#include "core/hle/result.h"
#include "core/hle/service/service.h"

namespace Kernel {
class ClientPort;
class ClientSession;
class ServerPort;
class SessionRequestHandler;
} // namespace Kernel

namespace Service {
namespace SM {

class SRV;

constexpr ResultCode ERR_SERVICE_NOT_REGISTERED(1, ErrorModule::SRV, ErrorSummary::WouldBlock,
                                                ErrorLevel::Temporary); // 0xD0406401
constexpr ResultCode ERR_MAX_CONNECTIONS_REACHED(2, ErrorModule::SRV, ErrorSummary::WouldBlock,
                                                 ErrorLevel::Temporary); // 0xD0406402
constexpr ResultCode ERR_INVALID_NAME_SIZE(5, ErrorModule::SRV, ErrorSummary::WrongArgument,
                                           ErrorLevel::Permanent); // 0xD9006405
constexpr ResultCode ERR_ACCESS_DENIED(6, ErrorModule::SRV, ErrorSummary::InvalidArgument,
                                       ErrorLevel::Permanent); // 0xD8E06406
constexpr ResultCode ERR_NAME_CONTAINS_NUL(7, ErrorModule::SRV, ErrorSummary::WrongArgument,
                                           ErrorLevel::Permanent); // 0xD9006407
constexpr ResultCode ERR_ALREADY_REGISTERED(ErrorDescription::AlreadyExists, ErrorModule::OS,
                                            ErrorSummary::WrongArgument,
                                            ErrorLevel::Permanent); // 0xD9001BFC

class ServiceManager {
public:
    static void InstallInterfaces(std::shared_ptr<ServiceManager> self);

    ResultVal<Kernel::SharedPtr<Kernel::ServerPort>> RegisterService(std::string name,
                                                                     unsigned int max_sessions);
    ResultVal<Kernel::SharedPtr<Kernel::ClientPort>> GetServicePort(const std::string& name);
    ResultVal<Kernel::SharedPtr<Kernel::ClientSession>> ConnectToService(const std::string& name);

private:
    std::weak_ptr<SRV> srv_interface;

    /// Map of registered services, retrieved using GetServicePort or ConnectToService.
    std::unordered_map<std::string, Kernel::SharedPtr<Kernel::ClientPort>> registered_services;
};

} // namespace SM
} // namespace Service
