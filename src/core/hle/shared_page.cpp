// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <cstring>
#include "core/core_timing.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/hle/shared_page.h"
#include "core/movie.h"
#include "core/settings.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace SharedPage {

static std::chrono::seconds GetInitTime() {
    u64 override_init_time = Core::Movie::GetInstance().GetOverrideInitTime();
    if (override_init_time) {
        // Override the clock init time with the one in the movie
        return std::chrono::seconds(override_init_time);
    }

    switch (Settings::values.init_clock) {
    case Settings::InitClock::SystemTime: {
        auto now = std::chrono::system_clock::now();
        // If the system time is in daylight saving, we give an additional hour to console time
        std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm* now_tm = std::localtime(&now_time_t);
        if (now_tm && now_tm->tm_isdst > 0)
            now = now + std::chrono::hours(1);
        return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
    }
    case Settings::InitClock::FixedTime:
        return std::chrono::seconds(Settings::values.init_time);
    }
}

Handler::Handler() {
    std::memset(&shared_page, 0, sizeof(shared_page));

    shared_page.running_hw = 0x1; // product

    // Some games wait until this value becomes 0x1, before asking running_hw
    shared_page.unknown_value = 0x1;

    // Set to a completely full battery
    shared_page.battery_state.charge_level.Assign(
        static_cast<u8>(Service::PTM::ChargeLevels::CompletelyFull));
    shared_page.battery_state.is_adapter_connected.Assign(1);
    shared_page.battery_state.is_charging.Assign(1);

    init_time = GetInitTime();

    using namespace std::placeholders;
    update_time_event = CoreTiming::RegisterEvent(
        "SharedPage::UpdateTimeCallback", std::bind(&Handler::UpdateTimeCallback, this, _1, _2));
    CoreTiming::ScheduleEvent(0, update_time_event);

    float slidestate =
        Settings::values.toggle_3d ? (float_le)Settings::values.factor_3d / 100 : 0.0f;
    shared_page.sliderstate_3d = slidestate;
}

/// Gets system time in 3DS format. The epoch is Jan 1900, and the unit is millisecond.
u64 Handler::GetSystemTime() const {
    std::chrono::milliseconds now =
        init_time +
        std::chrono::duration_cast<std::chrono::milliseconds>(CoreTiming::GetGlobalTimeUs());

    // 3DS system does't allow user to set a time before Jan 1 2000,
    // so we use it as an auxiliary epoch to calculate the console time.
    std::tm epoch_tm;
    epoch_tm.tm_sec = 0;
    epoch_tm.tm_min = 0;
    epoch_tm.tm_hour = 0;
    epoch_tm.tm_mday = 1;
    epoch_tm.tm_mon = 0;
    epoch_tm.tm_year = 100;
    epoch_tm.tm_isdst = 0;
    u64 epoch = std::mktime(&epoch_tm) * 1000;

    // 3DS console time uses Jan 1 1900 as internal epoch,
    // so we use the milliseconds between 1900 and 2000 as base console time
    u64 console_time = 3155673600000ULL;

    // Only when system time is after 2000, we set it as 3DS system time
    if (now.count() > epoch) {
        console_time += (now.count() - epoch);
    }

    return console_time;
}

void Handler::UpdateTimeCallback(u64 userdata, int cycles_late) {
    DateTime& date_time =
        shared_page.date_time_counter % 2 ? shared_page.date_time_0 : shared_page.date_time_1;

    date_time.date_time = GetSystemTime();
    date_time.update_tick = CoreTiming::GetTicks();
    date_time.tick_to_second_coefficient = BASE_CLOCK_RATE_ARM11;
    date_time.tick_offset = 0;

    ++shared_page.date_time_counter;

    // system time is updated hourly
    CoreTiming::ScheduleEvent(msToCycles(60 * 60 * 1000) - cycles_late, update_time_event);
}

void Handler::SetMacAddress(const MacAddress& addr) {
    std::memcpy(shared_page.wifi_macaddr, addr.data(), sizeof(MacAddress));
}

void Handler::SetWifiLinkLevel(WifiLinkLevel level) {
    shared_page.wifi_link_level = static_cast<u8>(level);
}

void Handler::Set3DLed(u8 state) {
    shared_page.ledstate_3d = state;
}

SharedPageDef& Handler::GetSharedPage() {
    return shared_page;
}

} // namespace SharedPage
