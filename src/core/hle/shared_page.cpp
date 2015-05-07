// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>

#include "common/common_types.h"
#include "common/common_funcs.h"

#include "core/core.h"
#include "core/mem_map.h"
#include "core/hle/config_mem.h"
#include "core/hle/shared_page.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace SharedPage {

// see http://3dbrew.org/wiki/Configuration_Memory#Shared_Memory_Page_For_ARM11_Processes

#pragma pack(1)
struct DateTime {
    u64     date_time;      // 0x0
    u64     update_tick;    // 0x8
    INSERT_PADDING_BYTES(0x20 - 0x10); // 0x10
};

struct SharedPageDef {
    // most of these names are taken from the 3dbrew page linked above.
    u32    date_time_selector;  // 0x0
    u8     running_hw;          // 0x4
    u8     mcu_hw_info;         // 0x5: don't know what the acronyms mean
    INSERT_PADDING_BYTES(0x20 - 0x6); // 0x6
    DateTime date_time_0;       // 0x20
    DateTime date_time_1;       // 0x40
    u8      wifi_macaddr[6];    // 0x60
    u8      wifi_unknown1;      // 0x66: 3dbrew says these are "Likely wifi hardware related"
    u8      wifi_unknown2;      // 0x67
    INSERT_PADDING_BYTES(0x80 - 0x68); // 0x68
    float   sliderstate_3d;     // 0x80
    u8      ledstate_3d;        // 0x84
    INSERT_PADDING_BYTES(0xA0 - 0x85); // 0x85
    u64     menu_title_id;      // 0xA0
    u64     active_menu_title_id; // 0xA8
    INSERT_PADDING_BYTES(0x1000 - 0xB0); // 0xB0
};
#pragma pack()

static_assert(sizeof(DateTime) == 0x20, "Datetime size is wrong");
static_assert(sizeof(SharedPageDef) == Memory::SHARED_PAGE_SIZE, "Shared page structure size is wrong");

static SharedPageDef shared_page;

template <typename T>
inline void Read(T &var, const u32 addr) {
    u32 offset = addr - Memory::SHARED_PAGE_VADDR;
    var = *(reinterpret_cast<T*>(((uintptr_t)&shared_page) + offset));
}

// Explicitly instantiate template functions because we aren't defining this in the header:
template void Read<u64>(u64 &var, const u32 addr);
template void Read<u32>(u32 &var, const u32 addr);
template void Read<u16>(u16 &var, const u32 addr);
template void Read<u8>(u8 &var, const u32 addr);

void Set3DSlider(float amount) {
    std::memset(&shared_page, 0, sizeof(shared_page));

    shared_page.sliderstate_3d = amount;
    shared_page.ledstate_3d = (amount == 0.0f); // off when non-zero
}

void Init() {
    shared_page.running_hw = 0x1; // product
    Set3DSlider(0.0f);
}

void Shutdown() {
}

} // namespace
