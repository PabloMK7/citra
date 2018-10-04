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
#elif defined(__linux__)
#include <byteswap.h>
#elif defined(__Bitrig__) || defined(__DragonFly__) || defined(__FreeBSD__) ||                     \
    defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/endian.h>
#endif
#include <cstring>
#include "common/common_types.h"

// GCC 4.6+
#if __GNUC__ >= 5 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)

#if __BYTE_ORDER__ && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) && !defined(COMMON_LITTLE_ENDIAN)
#define COMMON_LITTLE_ENDIAN 1
#elif __BYTE_ORDER__ && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) && !defined(COMMON_BIG_ENDIAN)
#define COMMON_BIG_ENDIAN 1
#endif

// LLVM/clang
#elif __clang__

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
inline u16 swap16(u16 _data) {
    return _byteswap_ushort(_data);
}
inline u32 swap32(u32 _data) {
    return _byteswap_ulong(_data);
}
inline u64 swap64(u64 _data) {
    return _byteswap_uint64(_data);
}
#elif defined(ARCHITECTURE_ARM) && (__ARM_ARCH >= 6)
inline u16 swap16(u16 _data) {
    u32 data = _data;
    __asm__("rev16 %0, %1\n" : "=l"(data) : "l"(data));
    return (u16)data;
}
inline u32 swap32(u32 _data) {
    __asm__("rev %0, %1\n" : "=l"(_data) : "l"(_data));
    return _data;
}
inline u64 swap64(u64 _data) {
    return ((u64)swap32(_data) << 32) | swap32(_data >> 32);
}
#elif __linux__
inline u16 swap16(u16 _data) {
    return bswap_16(_data);
}
inline u32 swap32(u32 _data) {
    return bswap_32(_data);
}
inline u64 swap64(u64 _data) {
    return bswap_64(_data);
}
#elif __APPLE__
inline __attribute__((always_inline)) u16 swap16(u16 _data) {
    return (_data >> 8) | (_data << 8);
}
inline __attribute__((always_inline)) u32 swap32(u32 _data) {
    return __builtin_bswap32(_data);
}
inline __attribute__((always_inline)) u64 swap64(u64 _data) {
    return __builtin_bswap64(_data);
}
#elif defined(__Bitrig__) || defined(__OpenBSD__)
// redefine swap16, swap32, swap64 as inline functions
#undef swap16
#undef swap32
#undef swap64
inline u16 swap16(u16 _data) {
    return __swap16(_data);
}
inline u32 swap32(u32 _data) {
    return __swap32(_data);
}
inline u64 swap64(u64 _data) {
    return __swap64(_data);
}
#elif defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__)
inline u16 swap16(u16 _data) {
    return bswap16(_data);
}
inline u32 swap32(u32 _data) {
    return bswap32(_data);
}
inline u64 swap64(u64 _data) {
    return bswap64(_data);
}
#else
// Slow generic implementation.
inline u16 swap16(u16 data) {
    return (data >> 8) | (data << 8);
}
inline u32 swap32(u32 data) {
    return (swap16(data) << 16) | swap16(data >> 16);
}
inline u64 swap64(u64 data) {
    return ((u64)swap32(data) << 32) | swap32(data >> 32);
}
#endif

inline float swapf(float f) {
    static_assert(sizeof(u32) == sizeof(float), "float must be the same size as uint32_t.");

    u32 value;
    std::memcpy(&value, &f, sizeof(u32));

    value = swap32(value);
    std::memcpy(&f, &value, sizeof(u32));

    return f;
}

inline double swapd(double f) {
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
    T value = T();

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

    // Arithmetics + assignements
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

// Arithmetics + assignements
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

#if COMMON_LITTLE_ENDIAN
using u16_le = u16;
using u32_le = u32;
using u64_le = u64;

using s16_le = s16;
using s32_le = s32;
using s64_le = s64;

template <typename T>
using enum_le = std::enable_if_t<std::is_enum_v<T>, T>;

using float_le = float;
using double_le = double;

using u64_be = swap_struct_t<u64, swap_64_t<u64>>;
using s64_be = swap_struct_t<s64, swap_64_t<s64>>;

using u32_be = swap_struct_t<u32, swap_32_t<u32>>;
using s32_be = swap_struct_t<s32, swap_32_t<s32>>;

using u16_be = swap_struct_t<u16, swap_16_t<u16>>;
using s16_be = swap_struct_t<s16, swap_16_t<s16>>;

template <typename T>
using enum_be = swap_enum_t<T>;

using float_be = swap_struct_t<float, swap_float_t<float>>;
using double_be = swap_struct_t<double, swap_double_t<double>>;
#else

using u64_le = swap_struct_t<u64, swap_64_t<u64>>;
using s64_le = swap_struct_t<s64, swap_64_t<s64>>;

using u32_le = swap_struct_t<u32, swap_32_t<u32>>;
using s32_le = swap_struct_t<s32, swap_32_t<s32>>;

using u16_le = swap_struct_t<u16, swap_16_t<u16>>;
using s16_le = swap_struct_t<s16, swap_16_t<s16>>;

template <typename T>
using enum_le = swap_enum_t<T>;

using float_le = swap_struct_t<float, swap_float_t<float>>;
using double_le = swap_struct_t<double, swap_double_t<double>>;

using u16_be = u16;
using u32_be = u32;
using u64_be = u64;

using s16_be = s16;
using s32_be = s32;
using s64_be = s64;

template <typename T>
using enum_be = std::enable_if_t<std::is_enum_v<T>, T>;

using float_be = float;
using double_be = double;

#endif
