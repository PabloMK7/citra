// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

/**
 * The shared page stores various runtime configuration settings. This memory page is
 * read-only for user processes (there is a bit in the header that grants the process
 * write access, according to 3dbrew; this is not emulated)
 */

#include <chrono>
#include <ctime>
#include <memory>
#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/swap.h"
#include "core/memory.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace CoreTiming {
struct EventType;
}

namespace SharedPage {

// See http://3dbrew.org/wiki/Configuration_Memory#Shared_Memory_Page_For_ARM11_Processes

struct DateTime {
    u64_le date_time;                  // 0
    u64_le update_tick;                // 8
    u64_le tick_to_second_coefficient; // 10
    u64_le tick_offset;                // 18
};
static_assert(sizeof(DateTime) == 0x20, "Datetime size is wrong");

union BatteryState {
    u8 raw;
    BitField<0, 1, u8> is_adapter_connected;
    BitField<1, 1, u8> is_charging;
    BitField<2, 3, u8> charge_level;
};

using MacAddress = std::array<u8, 6>;

// Default MAC address in the Nintendo 3DS range
constexpr MacAddress DefaultMac = {0x40, 0xF4, 0x07, 0x00, 0x00, 0x00};

enum class WifiLinkLevel : u8 {
    OFF = 0,
    POOR = 1,
    GOOD = 2,
    BEST = 3,
};

struct SharedPageDef {
    // Most of these names are taken from the 3dbrew page linked above.
    u32_le date_time_counter; // 0
    u8 running_hw;            // 4
    /// "Microcontroller hardware info"
    u8 mcu_hw_info;                      // 5
    INSERT_PADDING_BYTES(0x20 - 0x6);    // 6
    DateTime date_time_0;                // 20
    DateTime date_time_1;                // 40
    u8 wifi_macaddr[6];                  // 60
    u8 wifi_link_level;                  // 66
    u8 wifi_unknown2;                    // 67
    INSERT_PADDING_BYTES(0x80 - 0x68);   // 68
    float_le sliderstate_3d;             // 80
    u8 ledstate_3d;                      // 84
    BatteryState battery_state;          // 85
    u8 unknown_value;                    // 86
    INSERT_PADDING_BYTES(0xA0 - 0x87);   // 87
    u64_le menu_title_id;                // A0
    u64_le active_menu_title_id;         // A8
    INSERT_PADDING_BYTES(0x1000 - 0xB0); // B0
};
static_assert(sizeof(SharedPageDef) == Memory::SHARED_PAGE_SIZE,
              "Shared page structure size is wrong");

class Handler {
public:
    Handler();

    void SetMacAddress(const MacAddress&);

    void SetWifiLinkLevel(WifiLinkLevel);

    void Set3DLed(u8);

    SharedPageDef& GetSharedPage();

private:
    u64 GetSystemTime() const;
    void UpdateTimeCallback(u64 userdata, int cycles_late);
    CoreTiming::EventType* update_time_event;
    std::chrono::seconds init_time;

    SharedPageDef shared_page;
};

} // namespace SharedPage
