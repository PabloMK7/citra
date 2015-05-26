// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"

#include "core/core_timing.h"
#include "core/hle/applets/applet.h"
#include "core/hle/applets/swkbd.h"

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
static u32 applet_update_event = -1; ///< The CoreTiming event identifier for the Applet update callback.
/// The interval at which the Applet update callback will be called.
static const u64 applet_update_interval_microseconds = 16666;
std::shared_ptr<Applet> g_current_applet = nullptr; ///< The applet that is currently executing

ResultCode Applet::Create(Service::APT::AppletId id) {
    switch (id) {
    case Service::APT::AppletId::SoftwareKeyboard1:
    case Service::APT::AppletId::SoftwareKeyboard2:
        applets[id] = std::make_shared<SoftwareKeyboard>(id);
        break;
    default:
        // TODO(Subv): Find the right error code
        return ResultCode(ErrorDescription::NotFound, ErrorModule::Applet, ErrorSummary::NotSupported, ErrorLevel::Permanent);
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
static void AppletUpdateEvent(u64, int cycles_late) {
    if (g_current_applet && g_current_applet->IsRunning())
        g_current_applet->Update();

    CoreTiming::ScheduleEvent(usToCycles(applet_update_interval) - cycles_late,
        applet_update_event);
}

void Init() {
    applet_update_event = CoreTiming::RegisterEvent("HLE Applet Update Event", AppletUpdateEvent);
    CoreTiming::ScheduleEvent(usToCycles(applet_update_interval), applet_update_event);
}

void Shutdown() {
    CoreTiming::UnscheduleEvent(applet_update_event, 0);
}

}
} // namespace
