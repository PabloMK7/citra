// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "common/common_types.h"
#include "core/core.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace HLE {

extern bool g_reschedule;   ///< If true, immediately reschedules the CPU to a new thread

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

void Reschedule(const char *reason);

void Init();

void Shutdown();

} // namespace
