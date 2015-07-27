/*  armsupp.c -- ARMulator support code:  ARM6 Instruction Emulator.
    Copyright (C) 1994 Advanced RISC Machines Ltd.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#include "common/logging/log.h"

#include "core/mem_map.h"
#include "core/arm/skyeye_common/arm_regformat.h"
#include "core/arm/skyeye_common/armstate.h"
#include "core/arm/skyeye_common/armsupp.h"

// Unsigned sum of absolute difference
u8 ARMul_UnsignedAbsoluteDifference(u8 left, u8 right)
{
    if (left > right)
        return left - right;

    return right - left;
}

// Add with carry, indicates if a carry-out or signed overflow occurred.
u32 AddWithCarry(u32 left, u32 right, u32 carry_in, bool* carry_out_occurred, bool* overflow_occurred)
{
    u64 unsigned_sum = (u64)left + (u64)right + (u64)carry_in;
    s64 signed_sum = (s64)(s32)left + (s64)(s32)right + (s64)carry_in;
    u64 result = (unsigned_sum & 0xFFFFFFFF);

    if (carry_out_occurred)
        *carry_out_occurred = (result != unsigned_sum);

    if (overflow_occurred)
        *overflow_occurred = ((s64)(s32)result != signed_sum);

    return (u32)result;
}

// Compute whether an addition of A and B, giving RESULT, overflowed.
bool AddOverflow(u32 a, u32 b, u32 result)
{
    return ((NEG(a) && NEG(b) && POS(result)) ||
            (POS(a) && POS(b) && NEG(result)));
}

// Compute whether a subtraction of A and B, giving RESULT, overflowed.
bool SubOverflow(u32 a, u32 b, u32 result)
{
    return ((NEG(a) && POS(b) && POS(result)) ||
            (POS(a) && NEG(b) && NEG(result)));
}

// Returns true if the Q flag should be set as a result of overflow.
bool ARMul_AddOverflowQ(u32 a, u32 b)
{
    u32 result = a + b;
    if (((result ^ a) & (u32)0x80000000) && ((a ^ b) & (u32)0x80000000) == 0)
        return true;

    return false;
}

// 8-bit signed saturated addition
u8 ARMul_SignedSaturatedAdd8(u8 left, u8 right)
{
    u8 result = left + right;

    if (((result ^ left) & 0x80) && ((left ^ right) & 0x80) == 0) {
        if (left & 0x80)
            result = 0x80;
        else
            result = 0x7F;
    }

    return result;
}

// 8-bit signed saturated subtraction
u8 ARMul_SignedSaturatedSub8(u8 left, u8 right)
{
    u8 result = left - right;

    if (((result ^ left) & 0x80) && ((left ^ right) & 0x80) != 0) {
        if (left & 0x80)
            result = 0x80;
        else
            result = 0x7F;
    }

    return result;
}

// 16-bit signed saturated addition
u16 ARMul_SignedSaturatedAdd16(u16 left, u16 right)
{
    u16 result = left + right;

    if (((result ^ left) & 0x8000) && ((left ^ right) & 0x8000) == 0) {
        if (left & 0x8000)
            result = 0x8000;
        else
            result = 0x7FFF;
    }

    return result;
}

// 16-bit signed saturated subtraction
u16 ARMul_SignedSaturatedSub16(u16 left, u16 right)
{
    u16 result = left - right;

    if (((result ^ left) & 0x8000) && ((left ^ right) & 0x8000) != 0) {
        if (left & 0x8000)
            result = 0x8000;
        else
            result = 0x7FFF;
    }

    return result;
}

// 8-bit unsigned saturated addition
u8 ARMul_UnsignedSaturatedAdd8(u8 left, u8 right)
{
    u8 result = left + right;

    if (result < left)
        result = 0xFF;

    return result;
}

// 16-bit unsigned saturated addition
u16 ARMul_UnsignedSaturatedAdd16(u16 left, u16 right)
{
    u16 result = left + right;

    if (result < left)
        result = 0xFFFF;

    return result;
}

// 8-bit unsigned saturated subtraction
u8 ARMul_UnsignedSaturatedSub8(u8 left, u8 right)
{
    if (left <= right)
        return 0;

    return left - right;
}

// 16-bit unsigned saturated subtraction
u16 ARMul_UnsignedSaturatedSub16(u16 left, u16 right)
{
    if (left <= right)
        return 0;

    return left - right;
}

// Signed saturation.
u32 ARMul_SignedSatQ(s32 value, u8 shift, bool* saturation_occurred)
{
    const u32 max = (1 << shift) - 1;
    const s32 top = (value >> shift);

    if (top > 0) {
        *saturation_occurred = true;
        return max;
    }
    else if (top < -1) {
        *saturation_occurred = true;
        return ~max;
    }

    *saturation_occurred = false;
    return (u32)value;
}

// Unsigned saturation
u32 ARMul_UnsignedSatQ(s32 value, u8 shift, bool* saturation_occurred)
{
    const u32 max = (1 << shift) - 1;

    if (value < 0) {
        *saturation_occurred = true;
        return 0;
    } else if ((u32)value > max) {
        *saturation_occurred = true;
        return max;
    }

    *saturation_occurred = false;
    return (u32)value;
}
