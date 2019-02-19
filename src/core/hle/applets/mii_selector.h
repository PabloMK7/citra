// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "core/hle/applets/applet.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/result.h"
#include "core/hle/service/apt/apt.h"

namespace HLE::Applets {

struct MiiConfig {
    u8 enable_cancel_button;
    u8 enable_guest_mii;
    u8 show_on_top_screen;
    INSERT_PADDING_BYTES(5);
    u16 title[0x40];
    INSERT_PADDING_BYTES(4);
    u8 show_guest_miis;
    INSERT_PADDING_BYTES(3);
    u32 initially_selected_mii_index;
    u8 guest_mii_whitelist[6];
    u8 user_mii_whitelist[0x64];
    INSERT_PADDING_BYTES(2);
    u32 magic_value;
};
static_assert(sizeof(MiiConfig) == 0x104, "MiiConfig structure has incorrect size");
#define ASSERT_REG_POSITION(field_name, position)                                                  \
    static_assert(offsetof(MiiConfig, field_name) == position,                                     \
                  "Field " #field_name " has invalid position")
ASSERT_REG_POSITION(title, 0x08);
ASSERT_REG_POSITION(show_guest_miis, 0x8C);
ASSERT_REG_POSITION(initially_selected_mii_index, 0x90);
ASSERT_REG_POSITION(guest_mii_whitelist, 0x94);
#undef ASSERT_REG_POSITION

#pragma pack(push, 1)
struct MiiData {
    u32_be mii_id;
    u64_be system_id;
    u32_be specialness_and_creation_date;
    std::array<u8, 0x6> creator_mac;
    u16_be padding;
    u16_be mii_information;
    std::array<u16_le, 0xA> mii_name;
    u16_be width_height;
    union {
        u8 raw;

        BitField<0, 1, u8> disable_sharing;
        BitField<1, 4, u8> face_shape;
        BitField<5, 3, u8> skin_color;
    } appearance_bits1;
    union {
        u8 raw;

        BitField<0, 4, u8> wrinkles;
        BitField<4, 4, u8> makeup;
    } appearance_bits2;
    u8 hair_style;
    union {
        u8 raw;

        BitField<0, 3, u8> hair_color;
        BitField<3, 1, u8> flip_hair;
    } appearance_bits3;
    u32_be unknown1;
    union {
        u8 raw;

        BitField<0, 5, u8> eyebrow_style;
        BitField<5, 3, u8> eyebrow_color;
    } appearance_bits4;
    union {
        u8 raw;

        BitField<0, 4, u8> eyebrow_scale;
        BitField<4, 3, u8> eyebrow_yscale;
    } appearance_bits5;
    u16_be appearance_bits6;
    u32_be unknown2;
    u8 allow_copying;
    std::array<u8, 0x7> unknown3;
    std::array<u16_le, 0xA> author_name;
};
static_assert(sizeof(MiiData) == 0x5C, "MiiData structure has incorrect size");
#pragma pack(pop)

struct MiiResult {
    u32_be return_code;
    u32_be is_guest_mii_selected;
    u32_be selected_guest_mii_index;
    MiiData selected_mii_data;
    u16_be unknown1;
    u16_be mii_data_checksum;
    std::array<u16_le, 0xC> guest_mii_name;
};
static_assert(sizeof(MiiResult) == 0x84, "MiiResult structure has incorrect size");
#define ASSERT_REG_POSITION(field_name, position)                                                  \
    static_assert(offsetof(MiiResult, field_name) == position,                                     \
                  "Field " #field_name " has invalid position")
ASSERT_REG_POSITION(selected_mii_data, 0x0C);
ASSERT_REG_POSITION(guest_mii_name, 0x6C);
#undef ASSERT_REG_POSITION

class MiiSelector final : public Applet {
public:
    MiiSelector(Service::APT::AppletId id, std::weak_ptr<Service::APT::AppletManager> manager)
        : Applet(id, std::move(manager)) {}

    ResultCode ReceiveParameter(const Service::APT::MessageParameter& parameter) override;
    ResultCode StartImpl(const Service::APT::AppletStartupParameter& parameter) override;
    void Update() override;

private:
    /// This SharedMemory will be created when we receive the LibAppJustStarted message.
    /// It holds the framebuffer info retrieved by the application with
    /// GSPGPU::ImportDisplayCaptureInfo
    Kernel::SharedPtr<Kernel::SharedMemory> framebuffer_memory;

    MiiConfig config;
};
} // namespace HLE::Applets
