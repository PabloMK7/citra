// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <initializer_list>
#include <map>

#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/register_set.h"

namespace Pica {

struct Regs {
    enum Id : u32 {
        ViewportSizeX              =  0x41,
        ViewportInvSizeX           =  0x42,
        ViewportSizeY              =  0x43,
        ViewportInvSizeY           =  0x44,
        ViewportCorner             =  0x68,
        DepthBufferFormat          = 0x116,
        ColorBufferFormat          = 0x117,
        DepthBufferAddress         = 0x11C,
        ColorBufferAddress         = 0x11D,
        ColorBufferSize            = 0x11E,

        VertexArrayBaseAddr        = 0x200,
        VertexDescriptor           = 0x201, // 0x202
        VertexAttributeOffset      = 0x203, // 0x206,0x209,0x20C,0x20F,0x212,0x215,0x218,0x21B,0x21E,0x221,0x224
        VertexAttributeInfo0       = 0x204, // 0x207,0x20A,0x20D,0x210,0x213,0x216,0x219,0x21C,0x21F,0x222,0x225
        VertexAttributeInfo1       = 0x205, // 0x208,0x20B,0x20E,0x211,0x214,0x217,0x21A,0x21D,0x220,0x223,0x226

        NumIds                     = 0x300,
    };

    template<Id id>
    union Struct;
};

static inline Regs::Id VertexAttributeOffset(int n)
{
    return static_cast<Regs::Id>(0x203 + 3*n);
}

static inline Regs::Id VertexAttributeInfo0(int n)
{
    return static_cast<Regs::Id>(0x204 + 3*n);
}

static inline Regs::Id VertexAttributeInfo1(int n)
{
    return static_cast<Regs::Id>(0x205 + 3*n);
}

union CommandHeader {
    CommandHeader(u32 h) : hex(h) {}

    u32 hex;

    BitField< 0, 16, Regs::Id> cmd_id;
    BitField<16,  4, u32> parameter_mask;
    BitField<20, 11, u32> extra_data_length;
    BitField<31,  1, u32> group_commands;
};

static std::map<Regs::Id, const char*> command_names = {
    {Regs::ViewportSizeX, "ViewportSizeX" },
    {Regs::ViewportInvSizeX, "ViewportInvSizeX" },
    {Regs::ViewportSizeY, "ViewportSizeY" },
    {Regs::ViewportInvSizeY, "ViewportInvSizeY" },
    {Regs::ViewportCorner, "ViewportCorner" },
    {Regs::DepthBufferFormat, "DepthBufferFormat" },
    {Regs::ColorBufferFormat, "ColorBufferFormat" },
    {Regs::DepthBufferAddress, "DepthBufferAddress" },
    {Regs::ColorBufferAddress, "ColorBufferAddress" },
    {Regs::ColorBufferSize, "ColorBufferSize" },
};

template<>
union Regs::Struct<Regs::ViewportSizeX> {
    BitField<0, 24, u32> value;
};

template<>
union Regs::Struct<Regs::ViewportSizeY> {
    BitField<0, 24, u32> value;
};

template<>
union Regs::Struct<Regs::VertexDescriptor> {
    enum class Format : u64 {
        BYTE = 0,
        UBYTE = 1,
        SHORT = 2,
        FLOAT = 3,
    };

    BitField< 0,  2, Format> format0;
    BitField< 2,  2, u64> size0;      // number of elements minus 1
    BitField< 4,  2, Format> format1;
    BitField< 6,  2, u64> size1;
    BitField< 8,  2, Format> format2;
    BitField<10,  2, u64> size2;
    BitField<12,  2, Format> format3;
    BitField<14,  2, u64> size3;
    BitField<16,  2, Format> format4;
    BitField<18,  2, u64> size4;
    BitField<20,  2, Format> format5;
    BitField<22,  2, u64> size5;
    BitField<24,  2, Format> format6;
    BitField<26,  2, u64> size6;
    BitField<28,  2, Format> format7;
    BitField<30,  2, u64> size7;
    BitField<32,  2, Format> format8;
    BitField<34,  2, u64> size8;
    BitField<36,  2, Format> format9;
    BitField<38,  2, u64> size9;
    BitField<40,  2, Format> format10;
    BitField<42,  2, u64> size10;
    BitField<44,  2, Format> format11;
    BitField<46,  2, u64> size11;

    BitField<48, 12, u64> attribute_mask;
    BitField<60,  4, u64> num_attributes; // number of total attributes minus 1
};


} // namespace
