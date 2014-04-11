// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include "common/common_types.h"
#include "core/core.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

typedef void (*HLEFunc)();

struct HLEFunction {
	u32                 id;
	HLEFunc             func;
	const char*         name;
};

struct HLEModule {
	const char*         name;
	int                 num_funcs;
	const HLEFunction*  func_table;
};

#define PARAM(n)        Core::g_app_core->GetReg(n)
#define RETURN(n)       Core::g_app_core->SetReg(0, n)

namespace HLE {

void Init();

void Shutdown();

void RegisterModule(const char *name, int num_functions, const HLEFunction *func_table);

} // namespace
