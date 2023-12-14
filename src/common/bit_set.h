// This file is under the public domain.

#pragma once

#include <bit>
#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include "common/common_types.h"

namespace Common {

// Similar to std::bitset, this is a class which encapsulates a bitset, i.e.
// using the set bits of an integer to represent a set of integers.  Like that
// class, it acts like an array of bools:
//     BitSet32 bs;
//     bs[1] = true;
// but also like the underlying integer ([0] = least significant bit):
//     BitSet32 bs2 = ...;
//     bs = (bs ^ bs2) & BitSet32(0xffff);
// The following additional functionality is provided:
// - Construction using an initializer list.
//     BitSet bs { 1, 2, 4, 8 };
// - Efficiently iterating through the set bits:
//     for (int i : bs)
//         [i is the *index* of a set bit]
//   (This uses the appropriate CPU instruction to find the next set bit in one
//   operation.)
// - Counting set bits using .Count() - see comment on that method.

template <typename IntTy>
    requires std::is_unsigned_v<IntTy>
class BitSet {
public:
    // A reference to a particular bit, returned from operator[].
    class Ref {
    public:
        constexpr Ref(Ref&& other) : m_bs(other.m_bs), m_mask(other.m_mask) {}
        constexpr Ref(BitSet* bs, IntTy mask) : m_bs(bs), m_mask(mask) {}
        constexpr operator bool() const {
            return (m_bs->m_val & m_mask) != 0;
        }
        constexpr bool operator=(bool set) {
            m_bs->m_val = (m_bs->m_val & ~m_mask) | (set ? m_mask : 0);
            return set;
        }

    private:
        BitSet* m_bs;
        IntTy m_mask;
    };

    // A STL-like iterator is required to be able to use range-based for loops.
    class Iterator {
    public:
        constexpr Iterator(const Iterator& other) : m_val(other.m_val) {}
        constexpr Iterator(IntTy val) : m_val(val) {}
        constexpr int operator*() {
            // This will never be called when m_val == 0, because that would be the end() iterator
            return std::countr_zero(m_val);
        }
        constexpr Iterator& operator++() {
            // Unset least significant set bit
            m_val &= m_val - IntTy(1);
            return *this;
        }
        constexpr Iterator operator++(int) {
            Iterator other(*this);
            ++*this;
            return other;
        }
        constexpr bool operator==(Iterator other) const {
            return m_val == other.m_val;
        }
        constexpr bool operator!=(Iterator other) const {
            return m_val != other.m_val;
        }

    private:
        IntTy m_val;
    };

    constexpr BitSet() : m_val(0) {}
    constexpr explicit BitSet(IntTy val) : m_val(val) {}
    constexpr BitSet(std::initializer_list<int> init) {
        m_val = 0;
        for (int bit : init)
            m_val |= (IntTy)1 << bit;
    }

    constexpr static BitSet AllTrue(std::size_t count) {
        return BitSet(count == sizeof(IntTy) * 8 ? ~(IntTy)0 : (((IntTy)1 << count) - 1));
    }

    constexpr Ref operator[](std::size_t bit) {
        return Ref(this, (IntTy)1 << bit);
    }
    constexpr const Ref operator[](std::size_t bit) const {
        return (*const_cast<BitSet*>(this))[bit];
    }
    constexpr bool operator==(BitSet other) const {
        return m_val == other.m_val;
    }
    constexpr bool operator!=(BitSet other) const {
        return m_val != other.m_val;
    }
    constexpr bool operator<(BitSet other) const {
        return m_val < other.m_val;
    }
    constexpr bool operator>(BitSet other) const {
        return m_val > other.m_val;
    }
    constexpr BitSet operator|(BitSet other) const {
        return BitSet(m_val | other.m_val);
    }
    constexpr BitSet operator&(BitSet other) const {
        return BitSet(m_val & other.m_val);
    }
    constexpr BitSet operator^(BitSet other) const {
        return BitSet(m_val ^ other.m_val);
    }
    constexpr BitSet operator~() const {
        return BitSet(~m_val);
    }
    constexpr BitSet& operator|=(BitSet other) {
        return *this = *this | other;
    }
    constexpr BitSet& operator&=(BitSet other) {
        return *this = *this & other;
    }
    constexpr BitSet& operator^=(BitSet other) {
        return *this = *this ^ other;
    }
    operator u32() = delete;
    constexpr operator bool() {
        return m_val != 0;
    }
    constexpr u32 Count() const {
        return std::popcount(m_val);
    }

    constexpr Iterator begin() const {
        return Iterator(m_val);
    }
    constexpr Iterator end() const {
        return Iterator(0);
    }

    IntTy m_val;
};

} // namespace Common

typedef Common::BitSet<u8> BitSet8;
typedef Common::BitSet<u16> BitSet16;
typedef Common::BitSet<u32> BitSet32;
typedef Common::BitSet<u64> BitSet64;
