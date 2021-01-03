// Copyright (c) 2012- PPSSPP Project / Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#pragma once

#include <type_traits>

#if defined(_MSC_VER)
#include <cstdlib>
#endif
#include <cstring>
#include "common/common_types.h"

// GCC
#ifdef __GNUC__

#if __BYTE_ORDER__ && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) && !defined(COMMON_LITTLE_ENDIAN)
#define COMMON_LITTLE_ENDIAN 1
#elif __BYTE_ORDER__ && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) && !defined(COMMON_BIG_ENDIAN)
#define COMMON_BIG_ENDIAN 1
#endif

// LLVM/clang
#elif defined(__clang__)

#if __LITTLE_ENDIAN__ && !defined(COMMON_LITTLE_ENDIAN)
#define COMMON_LITTLE_ENDIAN 1
#elif __BIG_ENDIAN__ && !defined(COMMON_BIG_ENDIAN)
#define COMMON_BIG_ENDIAN 1
#endif

// MSVC
#elif defined(_MSC_VER) && !defined(COMMON_BIG_ENDIAN) && !defined(COMMON_LITTLE_ENDIAN)

#define COMMON_LITTLE_ENDIAN 1
#endif

// Worst case, default to little endian.
#if !COMMON_BIG_ENDIAN && !COMMON_LITTLE_ENDIAN
#define COMMON_LITTLE_ENDIAN 1
#endif

namespace Common {

#ifdef _MSC_VER
[[nodiscard]] inline u16 swap16(u16 data) noexcept {
    return _byteswap_ushort(data);
}
[[nodiscard]] inline u32 swap32(u32 data) noexcept {
    return _byteswap_ulong(data);
}
[[nodiscard]] inline u64 swap64(u64 data) noexcept {
    return _byteswap_uint64(data);
}
#elif defined(__clang__) || defined(__GNUC__)
#if defined(__Bitrig__) || defined(__OpenBSD__)
// redefine swap16, swap32, swap64 as inline functions
#undef swap16
#undef swap32
#undef swap64
#endif
[[nodiscard]] inline u16 swap16(u16 data) noexcept {
    return __builtin_bswap16(data);
}
[[nodiscard]] inline u32 swap32(u32 data) noexcept {
    return __builtin_bswap32(data);
}
[[nodiscard]] inline u64 swap64(u64 data) noexcept {
    return __builtin_bswap64(data);
}
#else
// Generic implementation.
[[nodiscard]] inline u16 swap16(u16 data) noexcept {
    return (data >> 8) | (data << 8);
}
[[nodiscard]] inline u32 swap32(u32 data) noexcept {
    return ((data & 0xFF000000U) >> 24) | ((data & 0x00FF0000U) >> 8) |
           ((data & 0x0000FF00U) << 8) | ((data & 0x000000FFU) << 24);
}
[[nodiscard]] inline u64 swap64(u64 data) noexcept {
    return ((data & 0xFF00000000000000ULL) >> 56) | ((data & 0x00FF000000000000ULL) >> 40) |
           ((data & 0x0000FF0000000000ULL) >> 24) | ((data & 0x000000FF00000000ULL) >> 8) |
           ((data & 0x00000000FF000000ULL) << 8) | ((data & 0x0000000000FF0000ULL) << 24) |
           ((data & 0x000000000000FF00ULL) << 40) | ((data & 0x00000000000000FFULL) << 56);
}
#endif

[[nodiscard]] inline float swapf(float f) noexcept {
    static_assert(sizeof(u32) == sizeof(float), "float must be the same size as uint32_t.");

    u32 value;
    std::memcpy(&value, &f, sizeof(u32));

    value = swap32(value);
    std::memcpy(&f, &value, sizeof(u32));

    return f;
}

[[nodiscard]] inline double swapd(double f) noexcept {
    static_assert(sizeof(u64) == sizeof(double), "double must be the same size as uint64_t.");

    u64 value;
    std::memcpy(&value, &f, sizeof(u64));

    value = swap64(value);
    std::memcpy(&f, &value, sizeof(u64));

    return f;
}

} // Namespace Common

template <typename T, typename F>
struct swap_struct_t {
    using swapped_t = swap_struct_t;

protected:
    T value;

    static T swap(T v) {
        return F::swap(v);
    }

public:
    T swap() const {
        return swap(value);
    }
    swap_struct_t() = default;
    swap_struct_t(const T& v) : value(swap(v)) {}

    template <typename S>
    swapped_t& operator=(const S& source) {
        value = swap(static_cast<T>(source));
        return *this;
    }

    operator s8() const {
        return static_cast<s8>(swap());
    }
    operator u8() const {
        return static_cast<u8>(swap());
    }
    operator s16() const {
        return static_cast<s16>(swap());
    }
    operator u16() const {
        return static_cast<u16>(swap());
    }
    operator s32() const {
        return static_cast<s32>(swap());
    }
    operator u32() const {
        return static_cast<u32>(swap());
    }
    operator s64() const {
        return static_cast<s64>(swap());
    }
    operator u64() const {
        return static_cast<u64>(swap());
    }
    operator float() const {
        return static_cast<float>(swap());
    }
    operator double() const {
        return static_cast<double>(swap());
    }

    // +v
    swapped_t operator+() const {
        return +swap();
    }
    // -v
    swapped_t operator-() const {
        return -swap();
    }

    // v / 5
    swapped_t operator/(const swapped_t& i) const {
        return swap() / i.swap();
    }
    template <typename S>
    swapped_t operator/(const S& i) const {
        return swap() / i;
    }

    // v * 5
    swapped_t operator*(const swapped_t& i) const {
        return swap() * i.swap();
    }
    template <typename S>
    swapped_t operator*(const S& i) const {
        return swap() * i;
    }

    // v + 5
    swapped_t operator+(const swapped_t& i) const {
        return swap() + i.swap();
    }
    template <typename S>
    swapped_t operator+(const S& i) const {
        return swap() + static_cast<T>(i);
    }
    // v - 5
    swapped_t operator-(const swapped_t& i) const {
        return swap() - i.swap();
    }
    template <typename S>
    swapped_t operator-(const S& i) const {
        return swap() - static_cast<T>(i);
    }

    // v += 5
    swapped_t& operator+=(const swapped_t& i) {
        value = swap(swap() + i.swap());
        return *this;
    }
    template <typename S>
    swapped_t& operator+=(const S& i) {
        value = swap(swap() + static_cast<T>(i));
        return *this;
    }
    // v -= 5
    swapped_t& operator-=(const swapped_t& i) {
        value = swap(swap() - i.swap());
        return *this;
    }
    template <typename S>
    swapped_t& operator-=(const S& i) {
        value = swap(swap() - static_cast<T>(i));
        return *this;
    }

    // ++v
    swapped_t& operator++() {
        value = swap(swap() + 1);
        return *this;
    }
    // --v
    swapped_t& operator--() {
        value = swap(swap() - 1);
        return *this;
    }

    // v++
    swapped_t operator++(int) {
        swapped_t old = *this;
        value = swap(swap() + 1);
        return old;
    }
    // v--
    swapped_t operator--(int) {
        swapped_t old = *this;
        value = swap(swap() - 1);
        return old;
    }
    // Comparaison
    // v == i
    bool operator==(const swapped_t& i) const {
        return swap() == i.swap();
    }
    template <typename S>
    bool operator==(const S& i) const {
        return swap() == i;
    }

    // v != i
    bool operator!=(const swapped_t& i) const {
        return swap() != i.swap();
    }
    template <typename S>
    bool operator!=(const S& i) const {
        return swap() != i;
    }

    // v > i
    bool operator>(const swapped_t& i) const {
        return swap() > i.swap();
    }
    template <typename S>
    bool operator>(const S& i) const {
        return swap() > i;
    }

    // v < i
    bool operator<(const swapped_t& i) const {
        return swap() < i.swap();
    }
    template <typename S>
    bool operator<(const S& i) const {
        return swap() < i;
    }

    // v >= i
    bool operator>=(const swapped_t& i) const {
        return swap() >= i.swap();
    }
    template <typename S>
    bool operator>=(const S& i) const {
        return swap() >= i;
    }

    // v <= i
    bool operator<=(const swapped_t& i) const {
        return swap() <= i.swap();
    }
    template <typename S>
    bool operator<=(const S& i) const {
        return swap() <= i;
    }

    // logical
    swapped_t operator!() const {
        return !swap();
    }

    // bitmath
    swapped_t operator~() const {
        return ~swap();
    }

    swapped_t operator&(const swapped_t& b) const {
        return swap() & b.swap();
    }
    template <typename S>
    swapped_t operator&(const S& b) const {
        return swap() & b;
    }
    swapped_t& operator&=(const swapped_t& b) {
        value = swap(swap() & b.swap());
        return *this;
    }
    template <typename S>
    swapped_t& operator&=(const S b) {
        value = swap(swap() & b);
        return *this;
    }

    swapped_t operator|(const swapped_t& b) const {
        return swap() | b.swap();
    }
    template <typename S>
    swapped_t operator|(const S& b) const {
        return swap() | b;
    }
    swapped_t& operator|=(const swapped_t& b) {
        value = swap(swap() | b.swap());
        return *this;
    }
    template <typename S>
    swapped_t& operator|=(const S& b) {
        value = swap(swap() | b);
        return *this;
    }

    swapped_t operator^(const swapped_t& b) const {
        return swap() ^ b.swap();
    }
    template <typename S>
    swapped_t operator^(const S& b) const {
        return swap() ^ b;
    }
    swapped_t& operator^=(const swapped_t& b) {
        value = swap(swap() ^ b.swap());
        return *this;
    }
    template <typename S>
    swapped_t& operator^=(const S& b) {
        value = swap(swap() ^ b);
        return *this;
    }

    template <typename S>
    swapped_t operator<<(const S& b) const {
        return swap() << b;
    }
    template <typename S>
    swapped_t& operator<<=(const S& b) const {
        value = swap(swap() << b);
        return *this;
    }

    template <typename S>
    swapped_t operator>>(const S& b) const {
        return swap() >> b;
    }
    template <typename S>
    swapped_t& operator>>=(const S& b) const {
        value = swap(swap() >> b);
        return *this;
    }

    // Member
    /** todo **/

    // Arithmetics
    template <typename S, typename T2, typename F2>
    friend S operator+(const S& p, const swapped_t v);

    template <typename S, typename T2, typename F2>
    friend S operator-(const S& p, const swapped_t v);

    template <typename S, typename T2, typename F2>
    friend S operator/(const S& p, const swapped_t v);

    template <typename S, typename T2, typename F2>
    friend S operator*(const S& p, const swapped_t v);

    template <typename S, typename T2, typename F2>
    friend S operator%(const S& p, const swapped_t v);

    // Arithmetics + assignments
    template <typename S, typename T2, typename F2>
    friend S operator+=(const S& p, const swapped_t v);

    template <typename S, typename T2, typename F2>
    friend S operator-=(const S& p, const swapped_t v);

    // Bitmath
    template <typename S, typename T2, typename F2>
    friend S operator&(const S& p, const swapped_t v);

    // Comparison
    template <typename S, typename T2, typename F2>
    friend bool operator<(const S& p, const swapped_t v);

    template <typename S, typename T2, typename F2>
    friend bool operator>(const S& p, const swapped_t v);

    template <typename S, typename T2, typename F2>
    friend bool operator<=(const S& p, const swapped_t v);

    template <typename S, typename T2, typename F2>
    friend bool operator>=(const S& p, const swapped_t v);

    template <typename S, typename T2, typename F2>
    friend bool operator!=(const S& p, const swapped_t v);

    template <typename S, typename T2, typename F2>
    friend bool operator==(const S& p, const swapped_t v);
};

// Arithmetics
template <typename S, typename T, typename F>
S operator+(const S& i, const swap_struct_t<T, F> v) {
    return i + v.swap();
}

template <typename S, typename T, typename F>
S operator-(const S& i, const swap_struct_t<T, F> v) {
    return i - v.swap();
}

template <typename S, typename T, typename F>
S operator/(const S& i, const swap_struct_t<T, F> v) {
    return i / v.swap();
}

template <typename S, typename T, typename F>
S operator*(const S& i, const swap_struct_t<T, F> v) {
    return i * v.swap();
}

template <typename S, typename T, typename F>
S operator%(const S& i, const swap_struct_t<T, F> v) {
    return i % v.swap();
}

// Arithmetics + assignments
template <typename S, typename T, typename F>
S& operator+=(S& i, const swap_struct_t<T, F> v) {
    i += v.swap();
    return i;
}

template <typename S, typename T, typename F>
S& operator-=(S& i, const swap_struct_t<T, F> v) {
    i -= v.swap();
    return i;
}

// Logical
template <typename S, typename T, typename F>
S operator&(const S& i, const swap_struct_t<T, F> v) {
    return i & v.swap();
}

template <typename S, typename T, typename F>
S operator&(const swap_struct_t<T, F> v, const S& i) {
    return static_cast<S>(v.swap() & i);
}

// Comparaison
template <typename S, typename T, typename F>
bool operator<(const S& p, const swap_struct_t<T, F> v) {
    return p < v.swap();
}
template <typename S, typename T, typename F>
bool operator>(const S& p, const swap_struct_t<T, F> v) {
    return p > v.swap();
}
template <typename S, typename T, typename F>
bool operator<=(const S& p, const swap_struct_t<T, F> v) {
    return p <= v.swap();
}
template <typename S, typename T, typename F>
bool operator>=(const S& p, const swap_struct_t<T, F> v) {
    return p >= v.swap();
}
template <typename S, typename T, typename F>
bool operator!=(const S& p, const swap_struct_t<T, F> v) {
    return p != v.swap();
}
template <typename S, typename T, typename F>
bool operator==(const S& p, const swap_struct_t<T, F> v) {
    return p == v.swap();
}

template <typename T>
struct swap_64_t {
    static T swap(T x) {
        return static_cast<T>(Common::swap64(x));
    }
};

template <typename T>
struct swap_32_t {
    static T swap(T x) {
        return static_cast<T>(Common::swap32(x));
    }
};

template <typename T>
struct swap_16_t {
    static T swap(T x) {
        return static_cast<T>(Common::swap16(x));
    }
};

template <typename T>
struct swap_float_t {
    static T swap(T x) {
        return static_cast<T>(Common::swapf(x));
    }
};

template <typename T>
struct swap_double_t {
    static T swap(T x) {
        return static_cast<T>(Common::swapd(x));
    }
};

template <typename T>
struct swap_enum_t {
    static_assert(std::is_enum_v<T>);
    using base = std::underlying_type_t<T>;

public:
    swap_enum_t() = default;
    swap_enum_t(const T& v) : value(swap(v)) {}

    swap_enum_t& operator=(const T& v) {
        value = swap(v);
        return *this;
    }

    operator T() const {
        return swap(value);
    }

    explicit operator base() const {
        return static_cast<base>(swap(value));
    }

protected:
    T value{};
    // clang-format off
    using swap_t = std::conditional_t<
        std::is_same_v<base, u16>, swap_16_t<u16>, std::conditional_t<
        std::is_same_v<base, s16>, swap_16_t<s16>, std::conditional_t<
        std::is_same_v<base, u32>, swap_32_t<u32>, std::conditional_t<
        std::is_same_v<base, s32>, swap_32_t<s32>, std::conditional_t<
        std::is_same_v<base, u64>, swap_64_t<u64>, std::conditional_t<
        std::is_same_v<base, s64>, swap_64_t<s64>, void>>>>>>;
    // clang-format on
    static T swap(T x) {
        return static_cast<T>(swap_t::swap(static_cast<base>(x)));
    }
};

struct SwapTag {}; // Use the different endianness from the system
struct KeepTag {}; // Use the same endianness as the system

template <typename T, typename Tag>
struct AddEndian;

// KeepTag specializations

template <typename T>
struct AddEndian<T, KeepTag> {
    using type = T;
};

// SwapTag specializations

template <>
struct AddEndian<u8, SwapTag> {
    using type = u8;
};

template <>
struct AddEndian<u16, SwapTag> {
    using type = swap_struct_t<u16, swap_16_t<u16>>;
};

template <>
struct AddEndian<u32, SwapTag> {
    using type = swap_struct_t<u32, swap_32_t<u32>>;
};

template <>
struct AddEndian<u64, SwapTag> {
    using type = swap_struct_t<u64, swap_64_t<u64>>;
};

template <>
struct AddEndian<s8, SwapTag> {
    using type = s8;
};

template <>
struct AddEndian<s16, SwapTag> {
    using type = swap_struct_t<s16, swap_16_t<s16>>;
};

template <>
struct AddEndian<s32, SwapTag> {
    using type = swap_struct_t<s32, swap_32_t<s32>>;
};

template <>
struct AddEndian<s64, SwapTag> {
    using type = swap_struct_t<s64, swap_64_t<s64>>;
};

template <>
struct AddEndian<float, SwapTag> {
    using type = swap_struct_t<float, swap_float_t<float>>;
};

template <>
struct AddEndian<double, SwapTag> {
    using type = swap_struct_t<double, swap_double_t<double>>;
};

template <typename T>
struct AddEndian<T, SwapTag> {
    static_assert(std::is_enum_v<T>);
    using type = swap_enum_t<T>;
};

// Alias LETag/BETag as KeepTag/SwapTag depending on the system
#if COMMON_LITTLE_ENDIAN

using LETag = KeepTag;
using BETag = SwapTag;

#else

using BETag = KeepTag;
using LETag = SwapTag;

#endif

// Aliases for LE types
using u16_le = AddEndian<u16, LETag>::type;
using u32_le = AddEndian<u32, LETag>::type;
using u64_le = AddEndian<u64, LETag>::type;

using s16_le = AddEndian<s16, LETag>::type;
using s32_le = AddEndian<s32, LETag>::type;
using s64_le = AddEndian<s64, LETag>::type;

template <typename T>
using enum_le = std::enable_if_t<std::is_enum_v<T>, typename AddEndian<T, LETag>::type>;

using float_le = AddEndian<float, LETag>::type;
using double_le = AddEndian<double, LETag>::type;

// Aliases for BE types
using u16_be = AddEndian<u16, BETag>::type;
using u32_be = AddEndian<u32, BETag>::type;
using u64_be = AddEndian<u64, BETag>::type;

using s16_be = AddEndian<s16, BETag>::type;
using s32_be = AddEndian<s32, BETag>::type;
using s64_be = AddEndian<s64, BETag>::type;

template <typename T>
using enum_be = std::enable_if_t<std::is_enum_v<T>, typename AddEndian<T, BETag>::type>;

using float_be = AddEndian<float, BETag>::type;
using double_be = AddEndian<double, BETag>::type;
