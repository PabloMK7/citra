// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>
#include "common/assert.h"
#include "core/core.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/result.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/sm/srv.h"

namespace Service::SM {

static ResultCode ValidateServiceName(const std::string& name) {
    if (name.size() <= 0 || name.size() > 8) {
        return ERR_INVALID_NAME_SIZE;
    }
    if (name.find('\0') != std::string::npos) {
        return ERR_NAME_CONTAINS_NUL;
    }
    return RESULT_SUCCESS;
}

ServiceManager::ServiceManager(Core::System& system) : system(system) {}

void ServiceManager::InstallInterfaces(Core::System& system) {
    ASSERT(system.ServiceManager().srv_interface.expired());

    auto srv = std::make_shared<SRV>(system);
    srv->InstallAsNamedPort(system.Kernel());
    system.ServiceManager().srv_interface = srv;
}

ResultVal<std::shared_ptr<Kernel::ServerPort>> ServiceManager::RegisterService(
    std::string name, unsigned int max_sessions) {

    CASCADE_CODE(ValidateServiceName(name));

    if (registered_services.find(name) != registered_services.end())
        return ERR_ALREADY_REGISTERED;

    auto [server_port, client_port] = system.Kernel().CreatePortPair(max_sessions, name);

    registered_services_inverse.emplace(client_port->GetObjectId(), name);
    registered_services.emplace(std::move(name), std::move(client_port));
    return MakeResult(std::move(server_port));
}

ResultVal<std::shared_ptr<Kernel::ClientPort>> ServiceManager::GetServicePort(
    const std::string& name) {

    CASCADE_CODE(ValidateServiceName(name));
    auto it = registered_services.find(name);
    if (it == registered_services.end()) {
        return ERR_SERVICE_NOT_REGISTERED;
    }

    return MakeResult(it->second);
}

ResultVal<std::shared_ptr<Kernel::ClientSession>> ServiceManager::ConnectToService(
    const std::string& name) {

    CASCADE_RESULT(auto client_port, GetServicePort(name));
    return client_port->Connect();
}

std::string ServiceManager::GetServiceNameByPortId(u32 port) const {
    if (registered_services_inverse.count(port)) {
        return registered_services_inverse.at(port);
    }

    return "";
}

} // namespace Service::SM
