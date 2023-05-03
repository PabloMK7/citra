// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstddef>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include "common/assert.h"
#include "common/common_types.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/applets/applet.h"
#include "core/hle/applets/erreula.h"
#include "core/hle/applets/mii_selector.h"
#include "core/hle/applets/mint.h"
#include "core/hle/applets/swkbd.h"
#include "core/hle/result.h"

namespace HLE::Applets {

static std::unordered_map<Service::APT::AppletId, std::shared_ptr<Applet>> applets;
/// The CoreTiming event identifier for the Applet update callback.
static Core::TimingEventType* applet_update_event = nullptr;
/// The interval at which the Applet update callback will be called, 16.6ms
static const u64 applet_update_interval_us = 16666;

ResultCode Applet::Create(Service::APT::AppletId id, Service::APT::AppletId parent, bool preload,
                          const std::shared_ptr<Service::APT::AppletManager>& manager) {
    switch (id) {
    case Service::APT::AppletId::SoftwareKeyboard1:
    case Service::APT::AppletId::SoftwareKeyboard2:
        applets[id] = std::make_shared<SoftwareKeyboard>(id, parent, preload, manager);
        break;
    case Service::APT::AppletId::Ed1:
    case Service::APT::AppletId::Ed2:
        applets[id] = std::make_shared<MiiSelector>(id, parent, preload, manager);
        break;
    case Service::APT::AppletId::Error:
    case Service::APT::AppletId::Error2:
        applets[id] = std::make_shared<ErrEula>(id, parent, preload, manager);
        break;
    case Service::APT::AppletId::Mint:
    case Service::APT::AppletId::Mint2:
        applets[id] = std::make_shared<Mint>(id, parent, preload, manager);
        break;
    default:
        LOG_ERROR(Service_APT, "Could not create applet {}", id);
        // TODO(Subv): Find the right error code
        return ResultCode(ErrorDescription::NotFound, ErrorModule::Applet,
                          ErrorSummary::NotSupported, ErrorLevel::Permanent);
    }

    Service::APT::AppletAttributes attributes;
    attributes.applet_pos.Assign(Service::APT::AppletPos::AutoLibrary);
    attributes.is_home_menu.Assign(false);
    const auto lock_handle_data = manager->GetLockHandle(attributes);

    manager->Initialize(id, lock_handle_data->corrected_attributes);
    manager->Enable(lock_handle_data->corrected_attributes);
    if (preload) {
        manager->FinishPreloadingLibraryApplet(id);
    }

    // Schedule the update event
    Core::System::GetInstance().CoreTiming().ScheduleEvent(
        usToCycles(applet_update_interval_us), applet_update_event, static_cast<u64>(id));
    return RESULT_SUCCESS;
}

std::shared_ptr<Applet> Applet::Get(Service::APT::AppletId id) {
    auto itr = applets.find(id);
    if (itr != applets.end())
        return itr->second;
    return nullptr;
}

/// Handles updating the current Applet every time it's called.
static void AppletUpdateEvent(u64 applet_id, s64 cycles_late) {
    const auto id = static_cast<Service::APT::AppletId>(applet_id);
    const auto applet = Applet::Get(id);
    ASSERT_MSG(applet != nullptr, "Applet doesn't exist! applet_id={:08X}", id);

    if (applet->IsActive()) {
        applet->Update();
    }

    // If the applet is still running after the last update, reschedule the event
    if (applet->IsRunning()) {
        Core::System::GetInstance().CoreTiming().ScheduleEvent(
            usToCycles(applet_update_interval_us) - cycles_late, applet_update_event, applet_id);
    } else {
        // Otherwise the applet has terminated, in which case we should clean it up
        applets[id] = nullptr;
    }
}

bool Applet::IsRunning() const {
    return is_running;
}

bool Applet::IsActive() const {
    return is_active;
}

ResultCode Applet::ReceiveParameter(const Service::APT::MessageParameter& parameter) {
    switch (parameter.signal) {
    case Service::APT::SignalType::Wakeup: {
        ResultCode result = Start(parameter);
        if (!result.IsError()) {
            is_active = true;
        }
        return result;
    }
    case Service::APT::SignalType::WakeupByCancel:
        return Finalize();
    default:
        return ReceiveParameterImpl(parameter);
    }
}

void Applet::SendParameter(const Service::APT::MessageParameter& parameter) {
    if (auto locked = manager.lock()) {
        locked->CancelAndSendParameter(parameter);
    } else {
        LOG_ERROR(Service_APT, "called after destructing applet manager");
    }
}

void Applet::CloseApplet(std::shared_ptr<Kernel::Object> object, const std::vector<u8>& buffer) {
    if (auto locked = manager.lock()) {
        locked->PrepareToCloseLibraryApplet(true, false, false);
        locked->CloseLibraryApplet(std::move(object), buffer);
    } else {
        LOG_ERROR(Service_APT, "called after destructing applet manager");
    }

    is_active = false;
    is_running = false;
}

void Init() {
    // Register the applet update callback
    applet_update_event = Core::System::GetInstance().CoreTiming().RegisterEvent(
        "HLE Applet Update Event", AppletUpdateEvent);
}

void Shutdown() {
    Core::System::GetInstance().CoreTiming().RemoveEvent(applet_update_event);
}
} // namespace HLE::Applets
