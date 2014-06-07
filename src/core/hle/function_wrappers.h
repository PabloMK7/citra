// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/mem_map.h"
#include "core/hle/hle.h"

namespace Wrap {

////////////////////////////////////////////////////////////////////////////////////////////////////
// Function wrappers that return type s32

namespace S32 {

template<s32 func(u32, u32, u32, u32)> void U32_U32_U32_U32() {
    RETURN(func(PARAM(0), PARAM(1), PARAM(2), PARAM(3)));
}

template<s32 func(u32, u32, u32, u32, u32)> void U32_U32_U32_U32_U32() {
    RETURN(func(PARAM(0), PARAM(1), PARAM(2), PARAM(3), PARAM(4)));
}

template<s32 func(u32*, u32, u32, u32, u32, u32)> void U32P_U32_U32_U32_U32_U32(){
    u32 param_1 = 0;
    u32 retval = func(&param_1, PARAM(0), PARAM(1), PARAM(2), PARAM(3), PARAM(4));
    Core::g_app_core->SetReg(1, param_1);
    RETURN(retval);
}

template<s32 func(s32*, u32*, s32, bool, s64)> void S32P_U32P_S32_Bool_S64() {
    s32 param_1 = 0;
    s32 retval = func(&param_1, (Handle*)Memory::GetPointer(PARAM(1)), (s32)PARAM(2), 
        (PARAM(3) != 0), (((s64)PARAM(4) << 32) | PARAM(0)));
    Core::g_app_core->SetReg(1, (u32)param_1);
    RETURN(retval);
}

// TODO(bunnei): Is this correct? Probably not
template<s32 func(u32, u32, u32, u32, s64)> void U32_U32_U32_U32_S64() {
    RETURN(func(PARAM(5), PARAM(1), PARAM(2), PARAM(3), (((s64)PARAM(4) << 32) | PARAM(0))));
}

template<s32 func(u32, s64)> void U32_S64() {
    RETURN(func(PARAM(0), (((s64)PARAM(3) << 32) | PARAM(2))));
}

template<s32 func(void*, void*, u32)> void VoidP_VoidP_U32(){
    RETURN(func(Memory::GetPointer(PARAM(0)), Memory::GetPointer(PARAM(1)), PARAM(2)));
}

template<s32 func(s32*, u32)> void S32P_U32(){
    s32 param_1 = 0;
    u32 retval = func(&param_1, PARAM(1));
    Core::g_app_core->SetReg(1, param_1);
    RETURN(retval);
}

template<s32 func(u32, s32)> void U32_S32() {
    RETURN(func(PARAM(0), (s32)PARAM(1)));
}

template<s32 func(u32*, u32)> void U32P_U32(){
    u32 param_1 = 0;
    u32 retval = func(&param_1, PARAM(1));
    Core::g_app_core->SetReg(1, param_1);
    RETURN(retval);
}

template<s32 func(u32)> void U32() {
    RETURN(func(PARAM(0)));
}

template<s32 func(void*)> void U32P() {
    RETURN(func(Memory::GetPointer(PARAM(0))));
}

template<s32 func(s64*, u32, void*, s32)> void S64P_U32_VoidP_S32(){
    RETURN(func((s64*)Memory::GetPointer(PARAM(0)), PARAM(1), Memory::GetPointer(PARAM(2)), 
        (s32)PARAM(3)));
}

template<s32 func(u32*, const char*)> void U32P_CharP() {
    u32 param_1 = 0;
    u32 retval = func(&param_1, Memory::GetCharPointer(PARAM(1)));
    Core::g_app_core->SetReg(1, param_1);
    RETURN(retval);
}

} // namespace S32

////////////////////////////////////////////////////////////////////////////////////////////////////
// Function wrappers that return type u32

namespace U32 {

template<u32 func()> void Void() {
    RETURN(func());
}

} // namespace U32

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Function wrappers that return type void

namespace Void {

template<void func(s64)> void S64() {
    func(((s64)PARAM(1) << 32) | PARAM(0));
}

template<void func(const char*)> void CharP() {
    func(Memory::GetCharPointer(PARAM(0)));
}

} // namespace Void

} // namespace Wrap
