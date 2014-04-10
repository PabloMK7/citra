// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include "common/common_types.h"
#include "core/core.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

typedef void (*HLEFunc)();
typedef void (*SysCallFunc)();

struct HLEFunction {
	u32                 id;
	HLEFunc             func;
	const char*         name;
	u32                 flags;
};

struct HLEModule {
	const char*         name;
	int                 num_funcs;
	const HLEFunction*  func_table;
};

struct SysCall {
    u8                  id;
	SysCallFunc         func;
    const char*         name;
};

#define PARAM(n)        Core::g_app_core->GetReg(n)
#define RETURN(n)       Core::g_app_core->SetReg(0, n)
