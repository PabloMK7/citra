// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/arm/arm_interface.h"
#include "core/core.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/svc.h"
#include "core/hle/result.h"
#include "core/memory.h"

namespace HLE {

static inline u32 Param(int n) {
    return Core::CPU().GetReg(n);
}

/**
 * HLE a function return from the current ARM11 userland process
 * @param res Result to return
 */
static inline void FuncReturn(u32 res) {
    Core::CPU().SetReg(0, res);
}

/**
 * HLE a function return (64-bit) from the current ARM11 userland process
 * @param res Result to return (64-bit)
 * @todo Verify that this function is correct
 */
static inline void FuncReturn64(u64 res) {
    Core::CPU().SetReg(0, (u32)(res & 0xFFFFFFFF));
    Core::CPU().SetReg(1, (u32)((res >> 32) & 0xFFFFFFFF));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Function wrappers that return type ResultCode

template <ResultCode func(u32, u32, u32, u32)>
void Wrap() {
    FuncReturn(func(Param(0), Param(1), Param(2), Param(3)).raw);
}

template <ResultCode func(u32*, u32, u32, u32, u32, u32)>
void Wrap() {
    u32 param_1 = 0;
    u32 retval = func(&param_1, Param(0), Param(1), Param(2), Param(3), Param(4)).raw;
    Core::CPU().SetReg(1, param_1);
    FuncReturn(retval);
}

template <ResultCode func(u32*, u32, u32, u32, u32, s32)>
void Wrap() {
    u32 param_1 = 0;
    u32 retval = func(&param_1, Param(0), Param(1), Param(2), Param(3), Param(4)).raw;
    Core::CPU().SetReg(1, param_1);
    FuncReturn(retval);
}

template <ResultCode func(s32*, VAddr, s32, bool, s64)>
void Wrap() {
    s32 param_1 = 0;
    s32 retval =
        func(&param_1, Param(1), (s32)Param(2), (Param(3) != 0), (((s64)Param(4) << 32) | Param(0)))
            .raw;

    Core::CPU().SetReg(1, (u32)param_1);
    FuncReturn(retval);
}

template <ResultCode func(s32*, VAddr, s32, u32)>
void Wrap() {
    s32 param_1 = 0;
    u32 retval = func(&param_1, Param(1), (s32)Param(2), Param(3)).raw;

    Core::CPU().SetReg(1, (u32)param_1);
    FuncReturn(retval);
}

template <ResultCode func(u32, u32, u32, u32, s64)>
void Wrap() {
    FuncReturn(
        func(Param(0), Param(1), Param(2), Param(3), (((s64)Param(5) << 32) | Param(4))).raw);
}

template <ResultCode func(u32*)>
void Wrap() {
    u32 param_1 = 0;
    u32 retval = func(&param_1).raw;
    Core::CPU().SetReg(1, param_1);
    FuncReturn(retval);
}

template <ResultCode func(u32, s64)>
void Wrap() {
    s32 retval = func(Param(0), (((s64)Param(3) << 32) | Param(2))).raw;

    FuncReturn(retval);
}

template <ResultCode func(Kernel::MemoryInfo*, Kernel::PageInfo*, u32)>
void Wrap() {
    Kernel::MemoryInfo memory_info = {};
    Kernel::PageInfo page_info = {};
    u32 retval = func(&memory_info, &page_info, Param(2)).raw;
    Core::CPU().SetReg(1, memory_info.base_address);
    Core::CPU().SetReg(2, memory_info.size);
    Core::CPU().SetReg(3, memory_info.permission);
    Core::CPU().SetReg(4, memory_info.state);
    Core::CPU().SetReg(5, page_info.flags);
    FuncReturn(retval);
}

template <ResultCode func(Kernel::MemoryInfo*, Kernel::PageInfo*, Kernel::Handle, u32)>
void Wrap() {
    Kernel::MemoryInfo memory_info = {};
    Kernel::PageInfo page_info = {};
    u32 retval = func(&memory_info, &page_info, Param(2), Param(3)).raw;
    Core::CPU().SetReg(1, memory_info.base_address);
    Core::CPU().SetReg(2, memory_info.size);
    Core::CPU().SetReg(3, memory_info.permission);
    Core::CPU().SetReg(4, memory_info.state);
    Core::CPU().SetReg(5, page_info.flags);
    FuncReturn(retval);
}

template <ResultCode func(s32*, u32)>
void Wrap() {
    s32 param_1 = 0;
    u32 retval = func(&param_1, Param(1)).raw;
    Core::CPU().SetReg(1, param_1);
    FuncReturn(retval);
}

template <ResultCode func(u32, s32)>
void Wrap() {
    FuncReturn(func(Param(0), (s32)Param(1)).raw);
}

template <ResultCode func(u32*, u32)>
void Wrap() {
    u32 param_1 = 0;
    u32 retval = func(&param_1, Param(1)).raw;
    Core::CPU().SetReg(1, param_1);
    FuncReturn(retval);
}

template <ResultCode func(u32)>
void Wrap() {
    FuncReturn(func(Param(0)).raw);
}

template <ResultCode func(u32*, s32, s32)>
void Wrap() {
    u32 param_1 = 0;
    u32 retval = func(&param_1, Param(1), Param(2)).raw;
    Core::CPU().SetReg(1, param_1);
    FuncReturn(retval);
}

template <ResultCode func(s32*, u32, s32)>
void Wrap() {
    s32 param_1 = 0;
    u32 retval = func(&param_1, Param(1), Param(2)).raw;
    Core::CPU().SetReg(1, param_1);
    FuncReturn(retval);
}

template <ResultCode func(s64*, u32, s32)>
void Wrap() {
    s64 param_1 = 0;
    u32 retval = func(&param_1, Param(1), Param(2)).raw;
    Core::CPU().SetReg(1, (u32)param_1);
    Core::CPU().SetReg(2, (u32)(param_1 >> 32));
    FuncReturn(retval);
}

template <ResultCode func(u32*, u32, u32, u32, u32)>
void Wrap() {
    u32 param_1 = 0;
    // The last parameter is passed in R0 instead of R4
    u32 retval = func(&param_1, Param(1), Param(2), Param(3), Param(0)).raw;
    Core::CPU().SetReg(1, param_1);
    FuncReturn(retval);
}

template <ResultCode func(u32, s64, s64)>
void Wrap() {
    s64 param1 = ((u64)Param(3) << 32) | Param(2);
    s64 param2 = ((u64)Param(4) << 32) | Param(1);
    FuncReturn(func(Param(0), param1, param2).raw);
}

template <ResultCode func(s64*, Kernel::Handle, u32)>
void Wrap() {
    s64 param_1 = 0;
    u32 retval = func(&param_1, Param(1), Param(2)).raw;
    Core::CPU().SetReg(1, (u32)param_1);
    Core::CPU().SetReg(2, (u32)(param_1 >> 32));
    FuncReturn(retval);
}

template <ResultCode func(Kernel::Handle, u32)>
void Wrap() {
    FuncReturn(func(Param(0), Param(1)).raw);
}

template <ResultCode func(Kernel::Handle*, Kernel::Handle*, VAddr, u32)>
void Wrap() {
    Kernel::Handle param_1 = 0;
    Kernel::Handle param_2 = 0;
    u32 retval = func(&param_1, &param_2, Param(2), Param(3)).raw;
    Core::CPU().SetReg(1, param_1);
    Core::CPU().SetReg(2, param_2);
    FuncReturn(retval);
}

template <ResultCode func(Kernel::Handle*, Kernel::Handle*)>
void Wrap() {
    Kernel::Handle param_1 = 0;
    Kernel::Handle param_2 = 0;
    u32 retval = func(&param_1, &param_2).raw;
    Core::CPU().SetReg(1, param_1);
    Core::CPU().SetReg(2, param_2);
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
    func(((s64)Param(1) << 32) | Param(0));
}

template <void func(VAddr, int len)>
void Wrap() {
    func(Param(0), Param(1));
}

template <void func(u8)>
void Wrap() {
    func((u8)Param(0));
}

} // namespace HLE
