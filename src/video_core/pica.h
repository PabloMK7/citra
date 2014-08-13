// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <initializer_list>
#include <map>

#include "common/bit_field.h"
#include "common/common_types.h"

#include "core/mem_map.h"

namespace Pica {

// Returns index corresponding to the Regs member labeled by field_name
// TODO: Due to Visual studio bug 209229, offsetof does not return constant expressions
//       when used with array elements (e.g. PICA_REG_INDEX(vs_uniform_setup.set_value[1])).
//       For details cf. https://connect.microsoft.com/VisualStudio/feedback/details/209229/offsetof-does-not-produce-a-constant-expression-for-array-members
//       Hopefully, this will be fixed sometime in the future.
//       For lack of better alternatives, we currently hardcode the offsets when constant
//       expressions are needed via PICA_REG_INDEX_WORKAROUND (on sane compilers, static_asserts
//       will then make sure the offsets indeed match the automatically calculated ones).
#define PICA_REG_INDEX(field_name) (offsetof(Pica::Regs, field_name) / sizeof(u32))
#if defined(_MSC_VER)
#define PICA_REG_INDEX_WORKAROUND(field_name, backup_workaround_index) (backup_workaround_index)
#else
// NOTE: Yeah, hacking in a static_assert here just to workaround the lacking MSVC compiler
//       really is this annoying. This macro just forwards its first argument to PICA_REG_INDEX
//       and then performs a (no-op) cast to size_t iff the second argument matches the expected
//       field offset. Otherwise, the compiler will fail to compile this code.
#define PICA_REG_INDEX_WORKAROUND(field_name, backup_workaround_index) \
    ((typename std::enable_if<backup_workaround_index == PICA_REG_INDEX(field_name), size_t>::type)PICA_REG_INDEX(field_name))
#endif // _MSC_VER

struct Regs {

// helper macro to properly align structure members.
// Calling INSERT_PADDING_WORDS will add a new member variable with a name like "pad121",
// depending on the current source line to make sure variable names are unique.
#define INSERT_PADDING_WORDS_HELPER1(x, y) x ## y
#define INSERT_PADDING_WORDS_HELPER2(x, y) INSERT_PADDING_WORDS_HELPER1(x, y)
#define INSERT_PADDING_WORDS(num_words) u32 INSERT_PADDING_WORDS_HELPER2(pad, __LINE__)[(num_words)];

    INSERT_PADDING_WORDS(0x41);

    BitField<0, 24, u32> viewport_size_x;
    INSERT_PADDING_WORDS(0x1);
    BitField<0, 24, u32> viewport_size_y;

    INSERT_PADDING_WORDS(0x9);

    BitField<0, 24, u32> viewport_depth_range; // float24
    BitField<0, 24, u32> viewport_depth_far_plane; // float24

    INSERT_PADDING_WORDS(0x1);

    union {
        // Maps components of output vertex attributes to semantics
        enum Semantic : u32
        {
            POSITION_X   =  0,
            POSITION_Y   =  1,
            POSITION_Z   =  2,
            POSITION_W   =  3,

            COLOR_R      =  8,
            COLOR_G      =  9,
            COLOR_B      = 10,
            COLOR_A      = 11,

            TEXCOORD0_U  = 12,
            TEXCOORD0_V  = 13,
            TEXCOORD1_U  = 14,
            TEXCOORD1_V  = 15,
            TEXCOORD2_U  = 22,
            TEXCOORD2_V  = 23,

            INVALID      = 31,
        };

        BitField< 0, 5, Semantic> map_x;
        BitField< 8, 5, Semantic> map_y;
        BitField<16, 5, Semantic> map_z;
        BitField<24, 5, Semantic> map_w;
    } vs_output_attributes[7];

    INSERT_PADDING_WORDS(0x11);

    union {
        BitField< 0, 16, u32> x;
        BitField<16, 16, u32> y;
    } viewport_corner;

    INSERT_PADDING_WORDS(0xa7);

    struct {
        enum ColorFormat : u32 {
            RGBA8    = 0,
            RGB8     = 1,
            RGBA5551 = 2,
            RGB565   = 3,
            RGBA4    = 4,
        };

        INSERT_PADDING_WORDS(0x6);

        u32 depth_format;
        u32 color_format;

        INSERT_PADDING_WORDS(0x4);

        u32 depth_buffer_address;
        u32 color_buffer_address;

        union {
            // Apparently, the framebuffer width is stored as expected,
            // while the height is stored as the actual height minus one.
            // Hence, don't access these fields directly but use the accessors
            // GetWidth() and GetHeight() instead.
            BitField< 0, 11, u32> width;
            BitField<12, 10, u32> height;
        };

        INSERT_PADDING_WORDS(0x1);

        inline u32 GetColorBufferAddress() const {
            return Memory::PhysicalToVirtualAddress(DecodeAddressRegister(color_buffer_address));
        }
        inline u32 GetDepthBufferAddress() const {
            return Memory::PhysicalToVirtualAddress(DecodeAddressRegister(depth_buffer_address));
        }

        inline u32 GetWidth() const {
            return width;
        }

        inline u32 GetHeight() const {
            return height + 1;
        }
    } framebuffer;

    INSERT_PADDING_WORDS(0xe0);

    struct {
        enum class Format : u64 {
            BYTE = 0,
            UBYTE = 1,
            SHORT = 2,
            FLOAT = 3,
        };

        BitField<0, 29, u32> base_address;

        inline u32 GetBaseAddress() const {
            // TODO: Ugly, should fix PhysicalToVirtualAddress instead
            return DecodeAddressRegister(base_address) - Memory::FCRAM_PADDR + Memory::HEAP_GSP_VADDR;
        }

        // Descriptor for internal vertex attributes
        union {
            BitField< 0,  2, Format> format0; // size of one element
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

            // number of total attributes minus 1
            BitField<60,  4, u64> num_extra_attributes;
        };

        inline Format GetFormat(int n) const {
            Format formats[] = {
                format0, format1, format2, format3,
                format4, format5, format6, format7,
                format8, format9, format10, format11
            };
            return formats[n];
        }

        inline int GetNumElements(int n) const {
            u64 sizes[] = {
                size0, size1, size2, size3,
                size4, size5, size6, size7,
                size8, size9, size10, size11
            };
            return (int)sizes[n]+1;
        }

        inline int GetElementSizeInBytes(int n) const {
            return (GetFormat(n) == Format::FLOAT) ? 4 :
                (GetFormat(n) == Format::SHORT) ? 2 : 1;
        }

        inline int GetStride(int n) const {
            return GetNumElements(n) * GetElementSizeInBytes(n);
        }

        inline int GetNumTotalAttributes() const {
            return (int)num_extra_attributes+1;
        }

        // Attribute loaders map the source vertex data to input attributes
        // This e.g. allows to load different attributes from different memory locations
        struct {
            // Source attribute data offset from the base address
            u32 data_offset;

            union {
                BitField< 0, 4, u64> comp0;
                BitField< 4, 4, u64> comp1;
                BitField< 8, 4, u64> comp2;
                BitField<12, 4, u64> comp3;
                BitField<16, 4, u64> comp4;
                BitField<20, 4, u64> comp5;
                BitField<24, 4, u64> comp6;
                BitField<28, 4, u64> comp7;
                BitField<32, 4, u64> comp8;
                BitField<36, 4, u64> comp9;
                BitField<40, 4, u64> comp10;
                BitField<44, 4, u64> comp11;

                // bytes for a single vertex in this loader
                BitField<48, 8, u64> byte_count;

                BitField<60, 4, u64> component_count;
            };

            inline int GetComponent(int n) const {
                u64 components[] = {
                    comp0, comp1, comp2, comp3,
                    comp4, comp5, comp6, comp7,
                    comp8, comp9, comp10, comp11
                };
                return (int)components[n];
            }
        } attribute_loaders[12];
    } vertex_attributes;

    struct {
        enum IndexFormat : u32 {
            BYTE = 0,
            SHORT = 1,
        };

        union {
            BitField<0, 31, u32> offset; // relative to base attribute address
            BitField<31, 1, IndexFormat> format;
        };
    } index_array;

    // Number of vertices to render
    u32 num_vertices;

    INSERT_PADDING_WORDS(0x5);

    // These two trigger rendering of triangles
    u32 trigger_draw;
    u32 trigger_draw_indexed;

    INSERT_PADDING_WORDS(0x2e);

    enum class TriangleTopology : u32 {
        List        = 0,
        Strip       = 1,
        Fan         = 2,
        ListIndexed = 3, // TODO: No idea if this is correct
    };

    BitField<8, 2, TriangleTopology> triangle_topology;

    INSERT_PADDING_WORDS(0x5b);

    // Offset to shader program entry point (in words)
    BitField<0, 16, u32> vs_main_offset;

    union {
        BitField< 0, 4, u64> attribute0_register;
        BitField< 4, 4, u64> attribute1_register;
        BitField< 8, 4, u64> attribute2_register;
        BitField<12, 4, u64> attribute3_register;
        BitField<16, 4, u64> attribute4_register;
        BitField<20, 4, u64> attribute5_register;
        BitField<24, 4, u64> attribute6_register;
        BitField<28, 4, u64> attribute7_register;
        BitField<32, 4, u64> attribute8_register;
        BitField<36, 4, u64> attribute9_register;
        BitField<40, 4, u64> attribute10_register;
        BitField<44, 4, u64> attribute11_register;
        BitField<48, 4, u64> attribute12_register;
        BitField<52, 4, u64> attribute13_register;
        BitField<56, 4, u64> attribute14_register;
        BitField<60, 4, u64> attribute15_register;

        int GetRegisterForAttribute(int attribute_index) {
            u64 fields[] = {
                attribute0_register,  attribute1_register,  attribute2_register,  attribute3_register,
                attribute4_register,  attribute5_register,  attribute6_register,  attribute7_register,
                attribute8_register,  attribute9_register,  attribute10_register, attribute11_register,
                attribute12_register, attribute13_register, attribute14_register, attribute15_register,
            };
            return (int)fields[attribute_index];
        }
    } vs_input_register_map;

    INSERT_PADDING_WORDS(0x3);

    struct {
        enum Format : u32
        {
            FLOAT24 = 0,
            FLOAT32 = 1
        };

        bool IsFloat32() const {
            return format == FLOAT32;
        }

        union {
            // Index of the next uniform to write to
            // TODO: ctrulib uses 8 bits for this, however that seems to yield lots of invalid indices
            BitField<0, 7, u32> index;

            BitField<31, 1, Format> format;
        };

        // Writing to these registers sets the "current" uniform.
        // TODO: It's not clear how the hardware stores what the "current" uniform is.
        u32 set_value[8];

    } vs_uniform_setup;

    INSERT_PADDING_WORDS(0x2);

    struct {
        u32 begin_load;

        // Writing to these registers sets the "current" word in the shader program.
        // TODO: It's not clear how the hardware stores what the "current" word is.
        u32 set_word[8];
    } vs_program;

    INSERT_PADDING_WORDS(0x1);

    // This register group is used to load an internal table of swizzling patterns,
    // which are indexed by each shader instruction to specify vector component swizzling.
    struct {
        u32 begin_load;

        // Writing to these registers sets the "current" swizzle pattern in the table.
        // TODO: It's not clear how the hardware stores what the "current" swizzle pattern is.
        u32 set_word[8];
    } vs_swizzle_patterns;

    INSERT_PADDING_WORDS(0x22);

#undef INSERT_PADDING_WORDS_HELPER1
#undef INSERT_PADDING_WORDS_HELPER2
#undef INSERT_PADDING_WORDS

    // Map register indices to names readable by humans
    // Used for debugging purposes, so performance is not an issue here
    static std::string GetCommandName(int index) {
        std::map<u32, std::string> map;
        Regs regs;

        // TODO: MSVC does not support using offsetof() on non-static data members even though this
        //       is technically allowed since C++11. Hence, this functionality is disabled until
        //       MSVC properly supports it.
        #ifndef _MSC_VER
        #define ADD_FIELD(name)                                                                               \
            do {                                                                                              \
                map.insert({PICA_REG_INDEX(name), #name});                                                    \
                for (u32 i = PICA_REG_INDEX(name) + 1; i < PICA_REG_INDEX(name) + sizeof(regs.name) / 4; ++i) \
                    map.insert({i, #name + std::string("+") + std::to_string(i-PICA_REG_INDEX(name))});       \
            } while(false)

        ADD_FIELD(viewport_size_x);
        ADD_FIELD(viewport_size_y);
        ADD_FIELD(viewport_depth_range);
        ADD_FIELD(viewport_depth_far_plane);
        ADD_FIELD(viewport_corner);
        ADD_FIELD(framebuffer);
        ADD_FIELD(vertex_attributes);
        ADD_FIELD(index_array);
        ADD_FIELD(num_vertices);
        ADD_FIELD(trigger_draw);
        ADD_FIELD(trigger_draw_indexed);
        ADD_FIELD(triangle_topology);
        ADD_FIELD(vs_main_offset);
        ADD_FIELD(vs_input_register_map);
        ADD_FIELD(vs_uniform_setup);
        ADD_FIELD(vs_program);
        ADD_FIELD(vs_swizzle_patterns);

        #undef ADD_FIELD
        #endif // _MSC_VER

        // Return empty string if no match is found
        return map[index];
    }

    static inline int NumIds() {
        return sizeof(Regs) / sizeof(u32);
    }

    u32& operator [] (int index) const {
        u32* content = (u32*)this;
        return content[index];
    }

    u32& operator [] (int index) {
        u32* content = (u32*)this;
        return content[index];
    }

private:
    /*
     * Most physical addresses which Pica registers refer to are 8-byte aligned.
     * This function should be used to get the address from a raw register value.
     */
    static inline u32 DecodeAddressRegister(u32 register_value) {
        return register_value * 8;
    }
};

// TODO: MSVC does not support using offsetof() on non-static data members even though this
//       is technically allowed since C++11. This macro should be enabled once MSVC adds
//       support for that.
#ifndef _MSC_VER
#define ASSERT_REG_POSITION(field_name, position) static_assert(offsetof(Regs, field_name) == position * 4, "Field "#field_name" has invalid position")

ASSERT_REG_POSITION(viewport_size_x, 0x41);
ASSERT_REG_POSITION(viewport_size_y, 0x43);
ASSERT_REG_POSITION(viewport_depth_range, 0x4d);
ASSERT_REG_POSITION(viewport_depth_far_plane, 0x4e);
ASSERT_REG_POSITION(vs_output_attributes[0], 0x50);
ASSERT_REG_POSITION(vs_output_attributes[1], 0x51);
ASSERT_REG_POSITION(viewport_corner, 0x68);
ASSERT_REG_POSITION(framebuffer, 0x110);
ASSERT_REG_POSITION(vertex_attributes, 0x200);
ASSERT_REG_POSITION(index_array, 0x227);
ASSERT_REG_POSITION(num_vertices, 0x228);
ASSERT_REG_POSITION(trigger_draw, 0x22e);
ASSERT_REG_POSITION(trigger_draw_indexed, 0x22f);
ASSERT_REG_POSITION(triangle_topology, 0x25e);
ASSERT_REG_POSITION(vs_main_offset, 0x2ba);
ASSERT_REG_POSITION(vs_input_register_map, 0x2bb);
ASSERT_REG_POSITION(vs_uniform_setup, 0x2c0);
ASSERT_REG_POSITION(vs_program, 0x2cb);
ASSERT_REG_POSITION(vs_swizzle_patterns, 0x2d5);

#undef ASSERT_REG_POSITION
#endif // !defined(_MSC_VER)

// The total number of registers is chosen arbitrarily, but let's make sure it's not some odd value anyway.
static_assert(sizeof(Regs) <= 0x300 * sizeof(u32), "Register set structure larger than it should be");
static_assert(sizeof(Regs) >= 0x300 * sizeof(u32), "Register set structure smaller than it should be");

extern Regs registers; // TODO: Not sure if we want to have one global instance for this


struct float24 {
    static float24 FromFloat32(float val) {
        float24 ret;
        ret.value = val;
        return ret;
    }

    // 16 bit mantissa, 7 bit exponent, 1 bit sign
    // TODO: No idea if this works as intended
    static float24 FromRawFloat24(u32 hex) {
        float24 ret;
        if ((hex & 0xFFFFFF) == 0) {
            ret.value = 0;
        } else {
            u32 mantissa = hex & 0xFFFF;
            u32 exponent = (hex >> 16) & 0x7F;
            u32 sign = hex >> 23;
            ret.value = powf(2.0f, (float)exponent-63.0f) * (1.0f + mantissa * powf(2.0f, -16.f));
            if (sign)
                ret.value = -ret.value;
        }
        return ret;
    }

    // Not recommended for anything but logging
    float ToFloat32() const {
        return value;
    }

    float24 operator * (const float24& flt) const {
        return float24::FromFloat32(ToFloat32() * flt.ToFloat32());
    }

    float24 operator / (const float24& flt) const {
        return float24::FromFloat32(ToFloat32() / flt.ToFloat32());
    }

    float24 operator + (const float24& flt) const {
        return float24::FromFloat32(ToFloat32() + flt.ToFloat32());
    }

    float24 operator - (const float24& flt) const {
        return float24::FromFloat32(ToFloat32() - flt.ToFloat32());
    }

    float24 operator - () const {
        return float24::FromFloat32(-ToFloat32());
    }

    bool operator < (const float24& flt) const {
        return ToFloat32() < flt.ToFloat32();
    }

    bool operator > (const float24& flt) const {
        return ToFloat32() > flt.ToFloat32();
    }

    bool operator >= (const float24& flt) const {
        return ToFloat32() >= flt.ToFloat32();
    }

    bool operator <= (const float24& flt) const {
        return ToFloat32() <= flt.ToFloat32();
    }

private:
    // Stored as a regular float, merely for convenience
    // TODO: Perform proper arithmetic on this!
    float value;
};

union CommandHeader {
    CommandHeader(u32 h) : hex(h) {}

    u32 hex;

    BitField< 0, 16, u32> cmd_id;
    BitField<16,  4, u32> parameter_mask;
    BitField<20, 11, u32> extra_data_length;
    BitField<31,  1, u32> group_commands;
};


} // namespace
