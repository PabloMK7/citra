// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/mem_map.h"
#include "core/hle/hle.h"

namespace HLE {

#define PARAM(n)    Core::g_app_core->GetReg(n)
#define RETURN(n)   Core::g_app_core->SetReg(0, n)

////////////////////////////////////////////////////////////////////////////////////////////////////
// Function wrappers that return type s32

template<s32 func(u32, u32, u32, u32)> void Wrap() {
    RETURN(func(PARAM(0), PARAM(1), PARAM(2), PARAM(3)));
}

template<s32 func(u32, u32, u32, u32, u32)> void Wrap() {
    RETURN(func(PARAM(0), PARAM(1), PARAM(2), PARAM(3), PARAM(4)));
}

template<s32 func(u32*, u32, u32, u32, u32, u32)> void Wrap(){
    u32 param_1 = 0;
    u32 retval = func(&param_1, PARAM(0), PARAM(1), PARAM(2), PARAM(3), PARAM(4));
    Core::g_app_core->SetReg(1, param_1);
    RETURN(retval);
}

template<s32 func(s32*, u32*, s32, bool, s64)> void Wrap() {
    s32 param_1 = 0;
    s32 retval = func(&param_1, (Handle*)Memory::GetPointer(PARAM(1)), (s32)PARAM(2), 
        (PARAM(3) != 0), (((s64)PARAM(4) << 32) | PARAM(0)));
    Core::g_app_core->SetReg(1, (u32)param_1);
    RETURN(retval);
}

// TODO(bunnei): Is this correct? Probably not
template<s32 func(u32, u32, u32, u32, s64)> void Wrap() {
    RETURN(func(PARAM(5), PARAM(1), PARAM(2), PARAM(3), (((s64)PARAM(4) << 32) | PARAM(0))));
}

template<s32 func(u32, s64)> void Wrap() {
    RETURN(func(PARAM(0), (((s64)PARAM(3) << 32) | PARAM(2))));
}

template<s32 func(void*, void*, u32)> void Wrap(){
    RETURN(func(Memory::GetPointer(PARAM(0)), Memory::GetPointer(PARAM(1)), PARAM(2)));
}

template<s32 func(s32*, u32)> void Wrap(){
    s32 param_1 = 0;
    u32 retval = func(&param_1, PARAM(1));
    Core::g_app_core->SetReg(1, param_1);
    RETURN(retval);
}

template<s32 func(u32, s32)> void Wrap() {
    RETURN(func(PARAM(0), (s32)PARAM(1)));
}

template<s32 func(u32*, u32)> void Wrap(){
    u32 param_1 = 0;
    u32 retval = func(&param_1, PARAM(1));
    Core::g_app_core->SetReg(1, param_1);
    RETURN(retval);
}

template<s32 func(u32)> void Wrap() {
    RETURN(func(PARAM(0)));
}

template<s32 func(void*)> void Wrap() {
    RETURN(func(Memory::GetPointer(PARAM(0))));
}

template<s32 func(s64*, u32, void*, s32)> void Wrap(){
    RETURN(func((s64*)Memory::GetPointer(PARAM(0)), PARAM(1), Memory::GetPointer(PARAM(2)), 
        (s32)PARAM(3)));
}

template<s32 func(u32*, const char*)> void Wrap() {
    u32 param_1 = 0;
    u32 retval = func(&param_1, Memory::GetCharPointer(PARAM(1)));
    Core::g_app_core->SetReg(1, param_1);
    RETURN(retval);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Function wrappers that return type u32

template<u32 func()> void Wrap() {
    RETURN(func());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Function wrappers that return type void

template<void func(s64)> void Wrap() {
    func(((s64)PARAM(1) << 32) | PARAM(0));
}

template<void func(const char*)> void Wrap() {
    func(Memory::GetCharPointer(PARAM(0)));
}

#undef PARAM
#undef RETURN

} // namespace HLE
