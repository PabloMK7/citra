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
#include "core/arm/skyeye_common/armdefs.h"
#include "core/arm/skyeye_common/arm_regformat.h"

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
bool AddOverflow(ARMword a, ARMword b, ARMword result)
{
    return ((NEG(a) && NEG(b) && POS(result)) ||
            (POS(a) && POS(b) && NEG(result)));
}

// Compute whether a subtraction of A and B, giving RESULT, overflowed.
bool SubOverflow(ARMword a, ARMword b, ARMword result)
{
    return ((NEG(a) && POS(b) && POS(result)) ||
            (POS(a) && NEG(b) && NEG(result)));
}

// Returns true if the Q flag should be set as a result of overflow.
bool ARMul_AddOverflowQ(ARMword a, ARMword b)
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

// Whether or not the given CPU is in big endian mode (E bit is set)
bool InBigEndianMode(ARMul_State* cpu)
{
    return (cpu->Cpsr & (1 << 9)) != 0;
}

// Whether or not the given CPU is in a mode other than user mode.
bool InAPrivilegedMode(ARMul_State* cpu)
{
    return (cpu->Mode != USER32MODE);
}

// Reads from the CP15 registers. Used with implementation of the MRC instruction.
// Note that since the 3DS does not have the hypervisor extensions, these registers
// are not implemented.
u32 ReadCP15Register(ARMul_State* cpu, u32 crn, u32 opcode_1, u32 crm, u32 opcode_2)
{
    // Unprivileged registers
    if (crn == 13 && opcode_1 == 0 && crm == 0)
    {
        if (opcode_2 == 2)
            return cpu->CP15[CP15_THREAD_UPRW];

        if (opcode_2 == 3)
            return cpu->CP15[CP15_THREAD_URO];
    }

    if (InAPrivilegedMode(cpu))
    {
        if (crn == 0 && opcode_1 == 0)
        {
            if (crm == 0)
            {
                if (opcode_2 == 0)
                    return cpu->CP15[CP15_MAIN_ID];

                if (opcode_2 == 1)
                    return cpu->CP15[CP15_CACHE_TYPE];

                if (opcode_2 == 3)
                    return cpu->CP15[CP15_TLB_TYPE];

                if (opcode_2 == 5)
                    return cpu->CP15[CP15_CPU_ID];
            }
            else if (crm == 1)
            {
                if (opcode_2 == 0)
                    return cpu->CP15[CP15_PROCESSOR_FEATURE_0];

                if (opcode_2 == 1)
                    return cpu->CP15[CP15_PROCESSOR_FEATURE_1];

                if (opcode_2 == 2)
                    return cpu->CP15[CP15_DEBUG_FEATURE_0];

                if (opcode_2 == 4)
                    return cpu->CP15[CP15_MEMORY_MODEL_FEATURE_0];

                if (opcode_2 == 5)
                    return cpu->CP15[CP15_MEMORY_MODEL_FEATURE_1];

                if (opcode_2 == 6)
                    return cpu->CP15[CP15_MEMORY_MODEL_FEATURE_2];

                if (opcode_2 == 7)
                    return cpu->CP15[CP15_MEMORY_MODEL_FEATURE_3];
            }
            else if (crm == 2)
            {
                if (opcode_2 == 0)
                    return cpu->CP15[CP15_ISA_FEATURE_0];

                if (opcode_2 == 1)
                    return cpu->CP15[CP15_ISA_FEATURE_1];

                if (opcode_2 == 2)
                    return cpu->CP15[CP15_ISA_FEATURE_2];

                if (opcode_2 == 3)
                    return cpu->CP15[CP15_ISA_FEATURE_3];

                if (opcode_2 == 4)
                    return cpu->CP15[CP15_ISA_FEATURE_4];
            }
        }

        if (crn == 1 && opcode_1 == 0 && crm == 0)
        {
            if (opcode_2 == 0)
                return cpu->CP15[CP15_CONTROL];

            if (opcode_2 == 1)
                return cpu->CP15[CP15_AUXILIARY_CONTROL];

            if (opcode_2 == 2)
                return cpu->CP15[CP15_COPROCESSOR_ACCESS_CONTROL];
        }

        if (crn == 2 && opcode_1 == 0 && crm == 0)
        {
            if (opcode_2 == 0)
                return cpu->CP15[CP15_TRANSLATION_BASE_TABLE_0];

            if (opcode_2 == 1)
                return cpu->CP15[CP15_TRANSLATION_BASE_TABLE_1];

            if (opcode_2 == 2)
                return cpu->CP15[CP15_TRANSLATION_BASE_CONTROL];
        }

        if (crn == 3 && opcode_1 == 0 && crm == 0 && opcode_2 == 0)
            return cpu->CP15[CP15_DOMAIN_ACCESS_CONTROL];

        if (crn == 5 && opcode_1 == 0 && crm == 0)
        {
            if (opcode_2 == 0)
                return cpu->CP15[CP15_FAULT_STATUS];

            if (opcode_2 == 1)
                return cpu->CP15[CP15_INSTR_FAULT_STATUS];
        }

        if (crn == 6 && opcode_1 == 0 && crm == 0)
        {
            if (opcode_2 == 0)
                return cpu->CP15[CP15_FAULT_ADDRESS];

            if (opcode_2 == 1)
                return cpu->CP15[CP15_WFAR];
        }

        if (crn == 7 && opcode_1 == 0 && crm == 4 && opcode_2 == 0)
            return cpu->CP15[CP15_PHYS_ADDRESS];

        if (crn == 9 && opcode_1 == 0 && crm == 0 && opcode_2 == 0)
            return cpu->CP15[CP15_DATA_CACHE_LOCKDOWN];

        if (crn == 10 && opcode_1 == 0)
        {
            if (crm == 0 && opcode_2 == 0)
                return cpu->CP15[CP15_TLB_LOCKDOWN];

            if (crm == 2)
            {
                if (opcode_2 == 0)
                    return cpu->CP15[CP15_PRIMARY_REGION_REMAP];

                if (opcode_2 == 1)
                    return cpu->CP15[CP15_NORMAL_REGION_REMAP];
            }
        }

        if (crn == 13 && crm == 0)
        {
            if (opcode_2 == 0)
                return cpu->CP15[CP15_PID];

            if (opcode_2 == 1)
                return cpu->CP15[CP15_CONTEXT_ID];

            if (opcode_2 == 4)
                return cpu->CP15[CP15_THREAD_PRW];
        }

        if (crn == 15)
        {
            if (opcode_1 == 0 && crm == 12)
            {
                if (opcode_2 == 0)
                    return cpu->CP15[CP15_PERFORMANCE_MONITOR_CONTROL];

                if (opcode_2 == 1)
                    return cpu->CP15[CP15_CYCLE_COUNTER];

                if (opcode_2 == 2)
                    return cpu->CP15[CP15_COUNT_0];

                if (opcode_2 == 3)
                    return cpu->CP15[CP15_COUNT_1];
            }

            if (opcode_1 == 5 && opcode_2 == 2)
            {
                if (crm == 5)
                    return cpu->CP15[CP15_MAIN_TLB_LOCKDOWN_VIRT_ADDRESS];

                if (crm == 6)
                    return cpu->CP15[CP15_MAIN_TLB_LOCKDOWN_PHYS_ADDRESS];

                if (crm == 7)
                    return cpu->CP15[CP15_MAIN_TLB_LOCKDOWN_ATTRIBUTE];
            }

            if (opcode_1 == 7 && crm == 1 && opcode_2 == 0)
                return cpu->CP15[CP15_TLB_DEBUG_CONTROL];
        }
    }

    LOG_ERROR(Core_ARM11, "MRC CRn=%u, CRm=%u, OP1=%u OP2=%u is not implemented. Returning zero.", crn, crm, opcode_1, opcode_2);
    return 0;
}

// Write to the CP15 registers. Used with implementation of the MCR instruction.
// Note that since the 3DS does not have the hypervisor extensions, these registers
// are not implemented.
void WriteCP15Register(ARMul_State* cpu, u32 value, u32 crn, u32 opcode_1, u32 crm, u32 opcode_2)
{
    if (InAPrivilegedMode(cpu))
    {
        if (crn == 1 && opcode_1 == 0 && crm == 0)
        {
            if (opcode_2 == 0)
                cpu->CP15[CP15_CONTROL] = value;
            else if (opcode_2 == 1)
                cpu->CP15[CP15_AUXILIARY_CONTROL] = value;
            else if (opcode_2 == 2)
                cpu->CP15[CP15_COPROCESSOR_ACCESS_CONTROL] = value;
        }
        else if (crn == 2 && opcode_1 == 0 && crm == 0)
        {
            if (opcode_2 == 0)
                cpu->CP15[CP15_TRANSLATION_BASE_TABLE_0] = value;
            else if (opcode_2 == 1)
                cpu->CP15[CP15_TRANSLATION_BASE_TABLE_1] = value;
            else if (opcode_2 == 2)
                cpu->CP15[CP15_TRANSLATION_BASE_CONTROL] = value;
        }
        else if (crn == 3 && opcode_1 == 0 && crm == 0 && opcode_2 == 0)
        {
            cpu->CP15[CP15_DOMAIN_ACCESS_CONTROL] = value;
        }
        else if (crn == 5 && opcode_1 == 0 && crm == 0)
        {
            if (opcode_2 == 0)
                cpu->CP15[CP15_FAULT_STATUS] = value;
            else if (opcode_2 == 1)
                cpu->CP15[CP15_INSTR_FAULT_STATUS] = value;
        }
        else if (crn == 6 && opcode_1 == 0 && crm == 0)
        {
            if (opcode_2 == 0)
                cpu->CP15[CP15_FAULT_ADDRESS] = value;
            else if (opcode_2 == 1)
                cpu->CP15[CP15_WFAR] = value;
        }
        else if (crn == 7 && opcode_1 == 0)
        {
            LOG_WARNING(Core_ARM11, "Cache operations are not fully implemented.");

            if (crm == 0 && opcode_2 == 4)
            {
                cpu->CP15[CP15_WAIT_FOR_INTERRUPT] = value;
            }
            else if (crm == 4 && opcode_2 == 0)
            {
                // NOTE: Not entirely accurate. This should do permission checks.
                cpu->CP15[CP15_PHYS_ADDRESS] = Memory::VirtualToPhysicalAddress(value);
            }
            else if (crm == 5)
            {
                if (opcode_2 == 0)
                    cpu->CP15[CP15_INVALIDATE_INSTR_CACHE] = value;
                else if (opcode_2 == 1)
                    cpu->CP15[CP15_INVALIDATE_INSTR_CACHE_USING_MVA] = value;
                else if (opcode_2 == 2)
                    cpu->CP15[CP15_INVALIDATE_INSTR_CACHE_USING_INDEX] = value;
                else if (opcode_2 == 6)
                    cpu->CP15[CP15_FLUSH_BRANCH_TARGET_CACHE] = value;
                else if (opcode_2 == 7)
                    cpu->CP15[CP15_FLUSH_BRANCH_TARGET_CACHE_ENTRY] = value;
            }
            else if (crm == 6)
            {
                if (opcode_2 == 0)
                    cpu->CP15[CP15_INVALIDATE_DATA_CACHE] = value;
                else if (opcode_2 == 1)
                    cpu->CP15[CP15_INVALIDATE_DATA_CACHE_LINE_USING_MVA] = value;
                else if (opcode_2 == 2)
                    cpu->CP15[CP15_INVALIDATE_DATA_CACHE_LINE_USING_INDEX] = value;
            }
            else if (crm == 7 && opcode_2 == 0)
            {
                cpu->CP15[CP15_INVALIDATE_DATA_AND_INSTR_CACHE] = value;
            }
            else if (crm == 10)
            {
                if (opcode_2 == 0)
                    cpu->CP15[CP15_CLEAN_DATA_CACHE] = value;
                else if (opcode_2 == 1)
                    cpu->CP15[CP15_CLEAN_DATA_CACHE_LINE_USING_MVA] = value;
                else if (opcode_2 == 2)
                    cpu->CP15[CP15_CLEAN_DATA_CACHE_LINE_USING_INDEX] = value;
            }
            else if (crm == 14)
            {
                if (opcode_2 == 0)
                    cpu->CP15[CP15_CLEAN_AND_INVALIDATE_DATA_CACHE] = value;
                else if (opcode_2 == 1)
                    cpu->CP15[CP15_CLEAN_AND_INVALIDATE_DATA_CACHE_LINE_USING_MVA] = value;
                else if (opcode_2 == 2)
                    cpu->CP15[CP15_CLEAN_AND_INVALIDATE_DATA_CACHE_LINE_USING_INDEX] = value;
            }
        }
        else if (crn == 8 && opcode_1 == 0)
        {
            LOG_WARNING(Core_ARM11, "TLB operations not fully implemented.");

            if (crm == 5)
            {
                if (opcode_2 == 0)
                    cpu->CP15[CP15_INVALIDATE_ITLB] = value;
                else if (opcode_2 == 1)
                    cpu->CP15[CP15_INVALIDATE_ITLB_SINGLE_ENTRY] = value;
                else if (opcode_2 == 2)
                    cpu->CP15[CP15_INVALIDATE_ITLB_ENTRY_ON_ASID_MATCH] = value;
                else if (opcode_2 == 3)
                    cpu->CP15[CP15_INVALIDATE_ITLB_ENTRY_ON_MVA] = value;
            }
            else if (crm == 6)
            {
                if (opcode_2 == 0)
                    cpu->CP15[CP15_INVALIDATE_DTLB] = value;
                else if (opcode_2 == 1)
                    cpu->CP15[CP15_INVALIDATE_DTLB_SINGLE_ENTRY] = value;
                else if (opcode_2 == 2)
                    cpu->CP15[CP15_INVALIDATE_DTLB_ENTRY_ON_ASID_MATCH] = value;
                else if (opcode_2 == 3)
                    cpu->CP15[CP15_INVALIDATE_DTLB_ENTRY_ON_MVA] = value;
            }
            else if (crm == 7)
            {
                if (opcode_2 == 0)
                    cpu->CP15[CP15_INVALIDATE_UTLB] = value;
                else if (opcode_2 == 1)
                    cpu->CP15[CP15_INVALIDATE_UTLB_SINGLE_ENTRY] = value;
                else if (opcode_2 == 2)
                    cpu->CP15[CP15_INVALIDATE_UTLB_ENTRY_ON_ASID_MATCH] = value;
                else if (opcode_2 == 3)
                    cpu->CP15[CP15_INVALIDATE_UTLB_ENTRY_ON_MVA] = value;
            }
        }
        else if (crn == 9 && opcode_1 == 0 && crm == 0 && opcode_2 == 0)
        {
            cpu->CP15[CP15_DATA_CACHE_LOCKDOWN] = value;
        }
        else if (crn == 10 && opcode_1 == 0)
        {
            if (crm == 0 && opcode_2 == 0)
            {
                cpu->CP15[CP15_TLB_LOCKDOWN] = value;
            }
            else if (crm == 2)
            {
                if (opcode_2 == 0)
                    cpu->CP15[CP15_PRIMARY_REGION_REMAP] = value;
                else if (opcode_2 == 1)
                    cpu->CP15[CP15_NORMAL_REGION_REMAP] = value;
            }
        }
        else if (crn == 13 && opcode_1 == 0 && crm == 0)
        {
            if (opcode_2 == 0)
                cpu->CP15[CP15_PID] = value;
            else if (opcode_2 == 1)
                cpu->CP15[CP15_CONTEXT_ID] = value;
            else if (opcode_2 == 3)
                cpu->CP15[CP15_THREAD_URO] = value;
            else if (opcode_2 == 4)
                cpu->CP15[CP15_THREAD_PRW] = value;
        }
        else if (crn == 15)
        {
            if (opcode_1 == 0 && crm == 12)
            {
                if (opcode_2 == 0)
                    cpu->CP15[CP15_PERFORMANCE_MONITOR_CONTROL] = value;
                else if (opcode_2 == 1)
                    cpu->CP15[CP15_CYCLE_COUNTER] = value;
                else if (opcode_2 == 2)
                    cpu->CP15[CP15_COUNT_0] = value;
                else if (opcode_2 == 3)
                    cpu->CP15[CP15_COUNT_1] = value;
            }
            else if (opcode_1 == 5)
            {
                if (crm == 4)
                {
                    if (opcode_2 == 2)
                        cpu->CP15[CP15_READ_MAIN_TLB_LOCKDOWN_ENTRY] = value;
                    else if (opcode_2 == 4)
                        cpu->CP15[CP15_WRITE_MAIN_TLB_LOCKDOWN_ENTRY] = value;
                }
                else if (crm == 5 && opcode_2 == 2)
                {
                    cpu->CP15[CP15_MAIN_TLB_LOCKDOWN_VIRT_ADDRESS] = value;
                }
                else if (crm == 6 && opcode_2 == 2)
                {
                    cpu->CP15[CP15_MAIN_TLB_LOCKDOWN_PHYS_ADDRESS] = value;
                }
                else if (crm == 7 && opcode_2 == 2)
                {
                    cpu->CP15[CP15_MAIN_TLB_LOCKDOWN_ATTRIBUTE] = value;
                }
            }
            else if (opcode_1 == 7 && crm == 1 && opcode_2 == 0)
            {
                cpu->CP15[CP15_TLB_DEBUG_CONTROL] = value;
            }
        }
    }

    // Unprivileged registers
    if (crn == 7 && opcode_1 == 0 && crm == 5 && opcode_2 == 4)
    {
        cpu->CP15[CP15_FLUSH_PREFETCH_BUFFER] = value;
    }
    else if (crn == 7 && opcode_1 == 0 && crm == 10)
    {
       if (opcode_2 == 4)
           cpu->CP15[CP15_DATA_SYNC_BARRIER] = value;
       else if (opcode_2 == 5)
           cpu->CP15[CP15_DATA_MEMORY_BARRIER] = value;
           
    }
    else if (crn == 13 && opcode_1 == 0 && crm == 0 && opcode_2 == 2)
    {
        cpu->CP15[CP15_THREAD_UPRW] = value;
    }
}
