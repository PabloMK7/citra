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

enum {
    CMD_BUFFER_ADDR     = 0xA0010000,    ///< Totally arbitrary unused address space
    CMD_BUFFER_SIZE     = 0x10000,
    CMD_BUFFER_MASK     = (CMD_BUFFER_SIZE - 1),
    CMD_BUFFER_ADDR_END = (CMD_BUFFER_ADDR + CMD_BUFFER_SIZE),
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

// Read from memory used by CTROS HLE functions
template <typename T>
inline void Read(T &var, const u32 addr);

// Write to memory used by CTROS HLE functions
template <typename T>
inline void Write(u32 addr, const T data);

u8* GetPointer(const u32 Address);

inline const char* GetCharPointer(const u32 address) {
    return (const char *)GetPointer(address);
}

void RegisterModule(std::string name, int num_functions, const FunctionDef *func_table);

void CallSyscall(u32 opcode);

void Init();

void Shutdown();

} // namespace
