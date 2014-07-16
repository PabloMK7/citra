// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "common/bit_field.h"
#include "common/register_set.h"

namespace GPU {

static const u32 kFrameCycles   = 268123480 / 60;   ///< 268MHz / 60 frames per second
static const u32 kFrameTicks    = kFrameCycles / 3; ///< Approximate number of instructions/frame

// MMIO region 0x1EFxxxxx
struct Regs {
    enum Id : u32 {
        MemoryFill                = 0x00004, // + 5,6,7; second block at 8-11

        FramebufferTop            = 0x00117, // + 11a,11b,11c,11d(?),11e...126
        FramebufferBottom         = 0x00157, // + 15a,15b,15c,15d(?),15e...166

        DisplayTransfer           = 0x00300, // + 301,302,303,304,305,306

        CommandProcessor          = 0x00638, // + 63a,63c

        NumIds                    = 0x01000
    };

    template<Id id>
    struct Struct;

    enum class FramebufferFormat : u32 {
        RGBA8  = 0,
        RGB8   = 1,
        RGB565 = 2,
        RGB5A1 = 3,
        RGBA4  = 4,
    };
};

template<>
struct Regs::Struct<Regs::MemoryFill> {
    u32 address_start;
    u32 address_end; // ?
    u32 size;
    u32 value; // ?

    inline u32 GetStartAddress() const {
        return address_start * 8;
    }

    inline u32 GetEndAddress() const {
        return address_end * 8;
    }
};
static_assert(sizeof(Regs::Struct<Regs::MemoryFill>) == 0x10, "Structure size and register block length don't match");

template<>
struct Regs::Struct<Regs::FramebufferTop> {
    using Format = Regs::FramebufferFormat;

    union {
        u32 size;

        BitField< 0, 16, u32> width;
        BitField<16, 16, u32> height;
    };

    u32 pad0[2];

    u32 address_left1;
    u32 address_left2;

    union {
        u32 format;

        BitField< 0, 3, Format> color_format;
    };

    u32 pad1;

    union {
        u32 active_fb;

        BitField<0, 1, u32> second_fb_active;
    };

    u32 pad2[5];

    u32 stride;

    u32 address_right1;
    u32 address_right2;
};

template<>
struct Regs::Struct<Regs::FramebufferBottom> : public Regs::Struct<Regs::FramebufferTop> {
};
static_assert(sizeof(Regs::Struct<Regs::FramebufferTop>) == 0x40, "Structure size and register block length don't match");

template<>
struct Regs::Struct<Regs::DisplayTransfer> {
    using Format = Regs::FramebufferFormat;

    u32 input_address;
    u32 output_address;

    inline u32 GetPhysicalInputAddress() const {
        return input_address * 8;
    }

    inline u32 GetPhysicalOutputAddress() const {
        return output_address * 8;
    }

    union {
        u32 output_size;

        BitField< 0, 16, u32> output_width;
        BitField<16, 16, u32> output_height;
    };

    union {
        u32 input_size;

        BitField< 0, 16, u32> input_width;
        BitField<16, 16, u32> input_height;
    };

    union {
        u32 flags;

        BitField< 0, 1, u32> flip_data;
        BitField< 8, 3, Format> input_format;
        BitField<12, 3, Format> output_format;
        BitField<16, 1, u32> output_tiled;
    };

    u32 unknown;
    u32 trigger;
};
static_assert(sizeof(Regs::Struct<Regs::DisplayTransfer>) == 0x1C, "Structure size and register block length don't match");

template<>
struct Regs::Struct<Regs::CommandProcessor> {
    u32 size;
    u32 pad0;
    u32 address;
    u32 pad1;
    u32 trigger;
};
static_assert(sizeof(Regs::Struct<Regs::CommandProcessor>) == 0x14, "Structure size and register block length don't match");


extern RegisterSet<u32, Regs> g_regs;

enum {
    TOP_ASPECT_X        = 0x5,
    TOP_ASPECT_Y        = 0x3,

    TOP_HEIGHT          = 240,
    TOP_WIDTH           = 400,
    BOTTOM_WIDTH        = 320,

    // Physical addresses in FCRAM (chosen arbitrarily)
    PADDR_TOP_LEFT_FRAME1       = 0x201D4C00,
    PADDR_TOP_LEFT_FRAME2       = 0x202D4C00,
    PADDR_TOP_RIGHT_FRAME1      = 0x203D4C00,
    PADDR_TOP_RIGHT_FRAME2      = 0x204D4C00,
    PADDR_SUB_FRAME1            = 0x205D4C00,
    PADDR_SUB_FRAME2            = 0x206D4C00,
    // Physical addresses in FCRAM used by ARM9 applications
/*    PADDR_TOP_LEFT_FRAME1       = 0x20184E60,
    PADDR_TOP_LEFT_FRAME2       = 0x201CB370,
    PADDR_TOP_RIGHT_FRAME1      = 0x20282160,
    PADDR_TOP_RIGHT_FRAME2      = 0x202C8670,
    PADDR_SUB_FRAME1            = 0x202118E0,
    PADDR_SUB_FRAME2            = 0x20249CF0,*/

    // Physical addresses in VRAM
    // TODO: These should just be deduced from the ones above
    PADDR_VRAM_TOP_LEFT_FRAME1  = 0x181D4C00,
    PADDR_VRAM_TOP_LEFT_FRAME2  = 0x182D4C00,
    PADDR_VRAM_TOP_RIGHT_FRAME1 = 0x183D4C00,
    PADDR_VRAM_TOP_RIGHT_FRAME2 = 0x184D4C00,
    PADDR_VRAM_SUB_FRAME1       = 0x185D4C00,
    PADDR_VRAM_SUB_FRAME2       = 0x186D4C00,
    // Physical addresses in VRAM used by ARM9 applications
/*    PADDR_VRAM_TOP_LEFT_FRAME2  = 0x181CB370,
    PADDR_VRAM_TOP_RIGHT_FRAME1 = 0x18282160,
    PADDR_VRAM_TOP_RIGHT_FRAME2 = 0x182C8670,
    PADDR_VRAM_SUB_FRAME1       = 0x182118E0,
    PADDR_VRAM_SUB_FRAME2       = 0x18249CF0,*/
};

/// Framebuffer location
enum FramebufferLocation {
    FRAMEBUFFER_LOCATION_UNKNOWN,   ///< Framebuffer location is unknown
    FRAMEBUFFER_LOCATION_FCRAM,     ///< Framebuffer is in the GSP heap
    FRAMEBUFFER_LOCATION_VRAM,      ///< Framebuffer is in VRAM
};

/**
 * Sets whether the framebuffers are in the GSP heap (FCRAM) or VRAM
 * @param
 */
void SetFramebufferLocation(const FramebufferLocation mode);

/**
 * Gets a read-only pointer to a framebuffer in memory
 * @param address Physical address of framebuffer
 * @return Returns const pointer to raw framebuffer
 */
const u8* GetFramebufferPointer(const u32 address);

u32 GetFramebufferAddr(const u32 address);

/**
 * Gets the location of the framebuffers
 */
FramebufferLocation GetFramebufferLocation(u32 address);

template <typename T>
inline void Read(T &var, const u32 addr);

template <typename T>
inline void Write(u32 addr, const T data);

/// Update hardware
void Update();

/// Initialize hardware
void Init();

/// Shutdown hardware
void Shutdown();


} // namespace
