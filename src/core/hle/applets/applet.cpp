// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstddef>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include "common/assert.h"
#include "common/common_types.h"
#include "core/core_timing.h"
#include "core/hle/applets/applet.h"
#include "core/hle/applets/erreula.h"
#include "core/hle/applets/mii_selector.h"
#include "core/hle/applets/swkbd.h"
#include "core/hle/result.h"
#include "core/hle/service/apt/apt.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

// Specializes std::hash for AppletId, so that we can use it in std::unordered_map.
// Workaround for libstdc++ bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=60970
namespace std {
template <>
struct hash<Service::APT::AppletId> {
    typedef Service::APT::AppletId argument_type;
    typedef std::size_t result_type;

    result_type operator()(const argument_type& id_code) const {
        typedef std::underlying_type<argument_type>::type Type;
        return std::hash<Type>()(static_cast<Type>(id_code));
    }
};
}

namespace HLE {
namespace Applets {

static std::unordered_map<Service::APT::AppletId, std::shared_ptr<Applet>> applets;
static u32 applet_update_event =
    -1; ///< The CoreTiming event identifier for the Applet update callback.
/// The interval at which the Applet update callback will be called, 16.6ms
static const u64 applet_update_interval_us = 16666;

ResultCode Applet::Create(Service::APT::AppletId id) {
    switch (id) {
    case Service::APT::AppletId::SoftwareKeyboard1:
    case Service::APT::AppletId::SoftwareKeyboard2:
        applets[id] = std::make_shared<SoftwareKeyboard>(id);
        break;
    case Service::APT::AppletId::Ed1:
    case Service::APT::AppletId::Ed2:
        applets[id] = std::make_shared<MiiSelector>(id);
        break;
    case Service::APT::AppletId::Error:
    case Service::APT::AppletId::Error2:
        applets[id] = std::make_shared<ErrEula>(id);
        break;
    default:
        LOG_ERROR(Service_APT, "Could not create applet %u", id);
        // TODO(Subv): Find the right error code
        return ResultCode(ErrorDescription::NotFound, ErrorModule::Applet,
                          ErrorSummary::NotSupported, ErrorLevel::Permanent);
    }

    return RESULT_SUCCESS;
}

std::shared_ptr<Applet> Applet::Get(Service::APT::AppletId id) {
    auto itr = applets.find(id);
    if (itr != applets.end())
        return itr->second;
    return nullptr;
}

/// Handles updating the current Applet every time it's called.
static void AppletUpdateEvent(u64 applet_id, int cycles_late) {
    Service::APT::AppletId id = static_cast<Service::APT::AppletId>(applet_id);
    std::shared_ptr<Applet> applet = Applet::Get(id);
    ASSERT_MSG(applet != nullptr, "Applet doesn't exist! applet_id=%08X", id);

    applet->Update();

    // If the applet is still running after the last update, reschedule the event
    if (applet->IsRunning()) {
        CoreTiming::ScheduleEvent(usToCycles(applet_update_interval_us) - cycles_late,
                                  applet_update_event, applet_id);
    } else {
        // Otherwise the applet has terminated, in which case we should clean it up
        applets[id] = nullptr;
    }
}

ResultCode Applet::Start(const Service::APT::AppletStartupParameter& parameter) {
    ResultCode result = StartImpl(parameter);
    if (result.IsError())
        return result;
    // Schedule the update event
    CoreTiming::ScheduleEvent(usToCycles(applet_update_interval_us), applet_update_event,
                              static_cast<u64>(id));
    return result;
}

bool IsLibraryAppletRunning() {
    // Check the applets map for instances of any applet
    for (auto itr = applets.begin(); itr != applets.end(); ++itr)
        if (itr->second != nullptr)
            return true;
    return false;
}

void Init() {
    // Register the applet update callback
    applet_update_event = CoreTiming::RegisterEvent("HLE Applet Update Event", AppletUpdateEvent);
}

void Shutdown() {
    CoreTiming::RemoveEvent(applet_update_event);
}
}
} // namespace
