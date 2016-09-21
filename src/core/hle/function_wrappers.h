// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/arm/arm_interface.h"
#include "core/core.h"
#include "core/hle/hle.h"
#include "core/hle/result.h"
#include "core/hle/svc.h"
#include "core/memory.h"

namespace HLE {

#define PARAM(n) Core::g_app_core->GetReg(n)

/// An invalid result code that is meant to be overwritten when a thread resumes from waiting
static const ResultCode RESULT_INVALID(0xDEADC0DE);

/**
 * HLE a function return from the current ARM11 userland process
 * @param res Result to return
 */
static inline void FuncReturn(u32 res) {
    Core::g_app_core->SetReg(0, res);
}

/**
 * HLE a function return (64-bit) from the current ARM11 userland process
 * @param res Result to return (64-bit)
 * @todo Verify that this function is correct
 */
static inline void FuncReturn64(u64 res) {
    Core::g_app_core->SetReg(0, (u32)(res & 0xFFFFFFFF));
    Core::g_app_core->SetReg(1, (u32)((res >> 32) & 0xFFFFFFFF));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Function wrappers that return type ResultCode

template <ResultCode func(u32, u32, u32, u32)>
void Wrap() {
    FuncReturn(func(PARAM(0), PARAM(1), PARAM(2), PARAM(3)).raw);
}

template <ResultCode func(u32*, u32, u32, u32, u32, u32)>
void Wrap() {
    u32 param_1 = 0;
    u32 retval = func(&param_1, PARAM(0), PARAM(1), PARAM(2), PARAM(3), PARAM(4)).raw;
    Core::g_app_core->SetReg(1, param_1);
    FuncReturn(retval);
}

template <ResultCode func(u32*, s32, u32, u32, u32, s32)>
void Wrap() {
    u32 param_1 = 0;
    u32 retval = func(&param_1, PARAM(0), PARAM(1), PARAM(2), PARAM(3), PARAM(4)).raw;
    Core::g_app_core->SetReg(1, param_1);
    FuncReturn(retval);
}

template <ResultCode func(s32*, u32*, s32, bool, s64)>
void Wrap() {
    s32 param_1 = 0;
    s32 retval = func(&param_1, (Handle*)Memory::GetPointer(PARAM(1)), (s32)PARAM(2),
                      (PARAM(3) != 0), (((s64)PARAM(4) << 32) | PARAM(0)))
                     .raw;

    if (retval != RESULT_INVALID.raw) {
        Core::g_app_core->SetReg(1, (u32)param_1);
        FuncReturn(retval);
    }
}

template <ResultCode func(u32, u32, u32, u32, s64)>
void Wrap() {
    FuncReturn(
        func(PARAM(0), PARAM(1), PARAM(2), PARAM(3), (((s64)PARAM(5) << 32) | PARAM(4))).raw);
}

template <ResultCode func(u32*)>
void Wrap() {
    u32 param_1 = 0;
    u32 retval = func(&param_1).raw;
    Core::g_app_core->SetReg(1, param_1);
    FuncReturn(retval);
}

template <ResultCode func(u32, s64)>
void Wrap() {
    s32 retval = func(PARAM(0), (((s64)PARAM(3) << 32) | PARAM(2))).raw;

    if (retval != RESULT_INVALID.raw) {
        FuncReturn(retval);
    }
}

template <ResultCode func(MemoryInfo*, PageInfo*, u32)>
void Wrap() {
    MemoryInfo memory_info = {};
    PageInfo page_info = {};
    u32 retval = func(&memory_info, &page_info, PARAM(2)).raw;
    Core::g_app_core->SetReg(1, memory_info.base_address);
    Core::g_app_core->SetReg(2, memory_info.size);
    Core::g_app_core->SetReg(3, memory_info.permission);
    Core::g_app_core->SetReg(4, memory_info.state);
    Core::g_app_core->SetReg(5, page_info.flags);
    FuncReturn(retval);
}

template <ResultCode func(MemoryInfo*, PageInfo*, Handle, u32)>
void Wrap() {
    MemoryInfo memory_info = {};
    PageInfo page_info = {};
    u32 retval = func(&memory_info, &page_info, PARAM(2), PARAM(3)).raw;
    Core::g_app_core->SetReg(1, memory_info.base_address);
    Core::g_app_core->SetReg(2, memory_info.size);
    Core::g_app_core->SetReg(3, memory_info.permission);
    Core::g_app_core->SetReg(4, memory_info.state);
    Core::g_app_core->SetReg(5, page_info.flags);
    FuncReturn(retval);
}

template <ResultCode func(s32*, u32)>
void Wrap() {
    s32 param_1 = 0;
    u32 retval = func(&param_1, PARAM(1)).raw;
    Core::g_app_core->SetReg(1, param_1);
    FuncReturn(retval);
}

template <ResultCode func(u32, s32)>
void Wrap() {
    FuncReturn(func(PARAM(0), (s32)PARAM(1)).raw);
}

template <ResultCode func(u32*, u32)>
void Wrap() {
    u32 param_1 = 0;
    u32 retval = func(&param_1, PARAM(1)).raw;
    Core::g_app_core->SetReg(1, param_1);
    FuncReturn(retval);
}

template <ResultCode func(u32)>
void Wrap() {
    FuncReturn(func(PARAM(0)).raw);
}

template <ResultCode func(s64*, u32, u32*, u32)>
void Wrap() {
    FuncReturn(func((s64*)Memory::GetPointer(PARAM(0)), PARAM(1),
                    (u32*)Memory::GetPointer(PARAM(2)), (s32)PARAM(3))
                   .raw);
}

template <ResultCode func(u32*, const char*)>
void Wrap() {
    u32 param_1 = 0;
    u32 retval = func(&param_1, (char*)Memory::GetPointer(PARAM(1))).raw;
    Core::g_app_core->SetReg(1, param_1);
    FuncReturn(retval);
}

template <ResultCode func(u32*, s32, s32)>
void Wrap() {
    u32 param_1 = 0;
    u32 retval = func(&param_1, PARAM(1), PARAM(2)).raw;
    Core::g_app_core->SetReg(1, param_1);
    FuncReturn(retval);
}

template <ResultCode func(s32*, u32, s32)>
void Wrap() {
    s32 param_1 = 0;
    u32 retval = func(&param_1, PARAM(1), PARAM(2)).raw;
    Core::g_app_core->SetReg(1, param_1);
    FuncReturn(retval);
}

template <ResultCode func(s64*, u32, s32)>
void Wrap() {
    s64 param_1 = 0;
    u32 retval = func(&param_1, PARAM(1), PARAM(2)).raw;
    Core::g_app_core->SetReg(1, (u32)param_1);
    Core::g_app_core->SetReg(2, (u32)(param_1 >> 32));
    FuncReturn(retval);
}

template <ResultCode func(u32*, u32, u32, u32, u32)>
void Wrap() {
    u32 param_1 = 0;
    // The last parameter is passed in R0 instead of R4
    u32 retval = func(&param_1, PARAM(1), PARAM(2), PARAM(3), PARAM(0)).raw;
    Core::g_app_core->SetReg(1, param_1);
    FuncReturn(retval);
}

template <ResultCode func(u32, s64, s64)>
void Wrap() {
    s64 param1 = ((u64)PARAM(3) << 32) | PARAM(2);
    s64 param2 = ((u64)PARAM(4) << 32) | PARAM(1);
    FuncReturn(func(PARAM(0), param1, param2).raw);
}

template <ResultCode func(s64*, Handle, u32)>
void Wrap() {
    s64 param_1 = 0;
    u32 retval = func(&param_1, PARAM(1), PARAM(2)).raw;
    Core::g_app_core->SetReg(1, (u32)param_1);
    Core::g_app_core->SetReg(2, (u32)(param_1 >> 32));
    FuncReturn(retval);
}

template <ResultCode func(Handle, u32)>
void Wrap() {
    FuncReturn(func(PARAM(0), PARAM(1)).raw);
}

template <ResultCode func(Handle*, Handle*, const char*, u32)>
void Wrap() {
    Handle param_1 = 0;
    Handle param_2 = 0;
    u32 retval = func(&param_1, &param_2,
                      reinterpret_cast<const char*>(Memory::GetPointer(PARAM(2))), PARAM(3))
                     .raw;
    // The first out parameter is moved into R2 and the second is moved into R1.
    Core::g_app_core->SetReg(1, param_2);
    Core::g_app_core->SetReg(2, param_1);
    FuncReturn(retval);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Function wrappers that return type u32

template <u32 func()>
void Wrap() {
    FuncReturn(func());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Function wrappers that return type s64

template <s64 func()>
void Wrap() {
    FuncReturn64(func());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Function wrappers that return type void

template <void func(s64)>
void Wrap() {
    func(((s64)PARAM(1) << 32) | PARAM(0));
}

template <void func(const char*)>
void Wrap() {
    func((char*)Memory::GetPointer(PARAM(0)));
}

template <void func(u8)>
void Wrap() {
    func((u8)PARAM(0));
}

#undef PARAM
#undef FuncReturn

} // namespace HLE
