// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#include <vector>

#include "core/hle/hle.h"
#include "core/hle/hle_syscall.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace HLE {

static std::vector<ModuleDef> g_module_db;

void RegisterModule(std::string name, int num_functions, const FunctionDef* func_table) {
    ModuleDef module = {name, num_functions, func_table};
    g_module_db.push_back(module);
}

void RegisterAllModules() {
    Register_SysCall();
}

void Init() {
    RegisterAllModules();
}

void Shutdown() {
	g_module_db.clear();
}

} // namespace
