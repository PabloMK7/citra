// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include "common/common_types.h"
#include "core/core.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

#define PARAM(n)        Core::g_app_core->GetReg(n)
#define RETURN(n)       Core::g_app_core->SetReg(0, n)

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace HLE {

enum {
    OS_THREAD_COMMAND_BUFFER_ADDR = 0xA0004000,
};

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

void CallSyscall(u32 opcode);

Addr CallGetThreadCommandBuffer();

void Init();

void Shutdown();

} // namespace
