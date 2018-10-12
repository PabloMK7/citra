// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>
#include "core/hle/kernel/kernel.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class HLERequestContext;
class Semaphore;
} // namespace Kernel

namespace Service::SM {

/// Interface to "srv:" service
class SRV final : public ServiceFramework<SRV> {
public:
    explicit SRV(Core::System& system);
    ~SRV();

private:
    void RegisterClient(Kernel::HLERequestContext& ctx);
    void EnableNotification(Kernel::HLERequestContext& ctx);
    void GetServiceHandle(Kernel::HLERequestContext& ctx);
    void Subscribe(Kernel::HLERequestContext& ctx);
    void Unsubscribe(Kernel::HLERequestContext& ctx);
    void PublishToSubscriber(Kernel::HLERequestContext& ctx);
    void RegisterService(Kernel::HLERequestContext& ctx);

    Core::System& system;
    Kernel::SharedPtr<Kernel::Semaphore> notification_semaphore;
    std::unordered_map<std::string, Kernel::SharedPtr<Kernel::Event>>
        get_service_handle_delayed_map;
};

} // namespace Service::SM
