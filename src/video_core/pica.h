// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <initializer_list>
#include <map>

#include "common/bit_field.h"
#include "common/common_types.h"

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

    INSERT_PADDING_WORDS(0x1bc);

    union {
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
    } vertex_descriptor;

    INSERT_PADDING_WORDS(0xfe);

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
        ADD_FIELD(vertex_descriptor);

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
ASSERT_REG_POSITION(vertex_descriptor, 0x200);

#undef ASSERT_REG_POSITION
#endif // !defined(_MSC_VER)

// The total number of registers is chosen arbitrarily, but let's make sure it's not some odd value anyway.
static_assert(sizeof(Regs) == 0x300 * sizeof(u32), "Invalid total size of register set");

union CommandHeader {
    CommandHeader(u32 h) : hex(h) {}

    u32 hex;

    BitField< 0, 16, u32> cmd_id;
    BitField<16,  4, u32> parameter_mask;
    BitField<20, 11, u32> extra_data_length;
    BitField<31,  1, u32> group_commands;
};


} // namespace
