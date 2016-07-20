// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

/**
 * The shared page stores various runtime configuration settings. This memory page is
 * read-only for user processes (there is a bit in the header that grants the process
 * write access, according to 3dbrew; this is not emulated)
 */

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/swap.h"

#include "core/memory.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace SharedPage {

// See http://3dbrew.org/wiki/Configuration_Memory#Shared_Memory_Page_For_ARM11_Processes

struct DateTime {
    u64_le date_time;                  // 0
    u64_le update_tick;                // 8
    u64_le tick_to_second_coefficient; // 10
    u64_le tick_offset;                // 18
};
static_assert(sizeof(DateTime) == 0x20, "Datetime size is wrong");

struct SharedPageDef {
    // Most of these names are taken from the 3dbrew page linked above.
    u32_le   date_time_counter;          // 0
    u8       running_hw;                 // 4
    /// "Microcontroller hardware info"
    u8       mcu_hw_info;                // 5
    INSERT_PADDING_BYTES(0x20 - 0x6);    // 6
    DateTime date_time_0;                // 20
    DateTime date_time_1;                // 40
    u8       wifi_macaddr[6];            // 60
    u8       wifi_link_level;            // 66
    u8       wifi_unknown2;              // 67
    INSERT_PADDING_BYTES(0x80 - 0x68);   // 68
    float_le sliderstate_3d;             // 80
    u8       ledstate_3d;                // 84
    INSERT_PADDING_BYTES(1);             // 85
    u8       unknown_value;              // 86
    INSERT_PADDING_BYTES(0xA0 - 0x87);   // 87
    u64_le   menu_title_id;              // A0
    u64_le   active_menu_title_id;       // A8
    INSERT_PADDING_BYTES(0x1000 - 0xB0); // B0
};
static_assert(sizeof(SharedPageDef) == Memory::SHARED_PAGE_SIZE, "Shared page structure size is wrong");

extern SharedPageDef shared_page;

void Init();

} // namespace
