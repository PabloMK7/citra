// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#include <vector>

#include "core/mem_map.h"
#include "core/hle/hle.h"
#include "core/hle/syscall.h"
#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace HLE {

static std::vector<ModuleDef> g_module_db;

u8* g_command_buffer = NULL;    ///< Command buffer used for sharing between appcore and syscore

// Read from memory used by CTROS HLE functions
template <typename T>
inline void Read(T &var, const u32 addr) {
    if (addr >= HLE::CMD_BUFFER_ADDR && addr < HLE::CMD_BUFFER_ADDR_END) {
        var = *((const T*)&g_command_buffer[addr & CMD_BUFFER_MASK]);
    } else {
        ERROR_LOG(HLE, "unknown read from address %08X", addr);
    }
}

// Write to memory used by CTROS HLE functions
template <typename T>
inline void Write(u32 addr, const T data) {
    if (addr >= HLE::CMD_BUFFER_ADDR && addr < HLE::CMD_BUFFER_ADDR_END) {
        *(T*)&g_command_buffer[addr & CMD_BUFFER_MASK] = data;
    } else {
        ERROR_LOG(HLE, "unknown write to address %08X", addr);
    }
}

u8 *GetPointer(const u32 addr) {
    if (addr >= HLE::CMD_BUFFER_ADDR && addr < HLE::CMD_BUFFER_ADDR_END) {
        return g_command_buffer + (addr & CMD_BUFFER_MASK);
    } else {
        ERROR_LOG(HLE, "unknown pointer from address %08X", addr);
        return 0;
    }
}

// Explicitly instantiate template functions because we aren't defining this in the header:

template void Read<u64>(u64 &var, const u32 addr);
template void Read<u32>(u32 &var, const u32 addr);
template void Read<u16>(u16 &var, const u32 addr);
template void Read<u8>(u8 &var, const u32 addr);

template void Write<u64>(u32 addr, const u64 data);
template void Write<u32>(u32 addr, const u32 data);
template void Write<u16>(u32 addr, const u16 data);
template void Write<u8>(u32 addr, const u8 data);

const FunctionDef* GetSyscallInfo(u32 opcode) {
    u32 func_num = opcode & 0xFFFFFF; // 8 bits
    if (func_num > 0xFF) {
        ERROR_LOG(HLE,"Unknown syscall: 0x%02X", func_num); 
        return NULL;
    }
    return &g_module_db[0].func_table[func_num];
}

void CallSyscall(u32 opcode) {
    const FunctionDef *info = GetSyscallInfo(opcode);

    if (!info) {
        return;
    }
    if (info->func) {
        info->func();
    } else {
        ERROR_LOG(HLE, "Unimplemented SysCall function %s(..)", info->name.c_str());
    }
}

void RegisterModule(std::string name, int num_functions, const FunctionDef* func_table) {
    ModuleDef module = {name, num_functions, func_table};
    g_module_db.push_back(module);
}

void RegisterAllModules() {
    Syscall::Register();
}

void Init() {
    Service::Init();

    g_command_buffer = new u8[CMD_BUFFER_SIZE];
    
    RegisterAllModules();

    NOTICE_LOG(HLE, "initialized OK");
}

void Shutdown() {
    Service::Shutdown();

    delete g_command_buffer;

    g_module_db.clear();

    NOTICE_LOG(HLE, "shutdown OK");
}

} // namespace
