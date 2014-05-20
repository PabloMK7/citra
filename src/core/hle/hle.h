// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include "common/common_types.h"
#include "core/core.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

#define PARAM(n)        Core::g_app_core->GetReg(n)
#define PARAM64(n)      (Core::g_app_core->GetReg(n) | ((u64)Core::g_app_core->GetReg(n + 1) << 32))
#define RETURN(n)       Core::g_app_core->SetReg(0, n)

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace HLE {

typedef u32 Addr;
typedef void (*Func)();

struct FunctionDef {
    u32                 id;
    Func                func;
    std::string         name;
};

struct ModuleDef {
    std::string         name;
    int                 num_funcs;
    const FunctionDef*  func_table;
};

void RegisterModule(std::string name, int num_functions, const FunctionDef *func_table);

void CallSVC(u32 opcode);

void EatCycles(u32 cycles);

void ReSchedule(const char *reason);

void Init();

void Shutdown();

} // namespace
