// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <vector>

#include "core/mem_map.h"
#include "core/hle/hle.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/service/service.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/cfg/cfg.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace HLE {

static std::vector<ModuleDef> g_module_db;

bool g_reschedule = false;  ///< If true, immediately reschedules the CPU to a new thread

const FunctionDef* GetSVCInfo(u32 opcode) {
    u32 func_num = opcode & 0xFFFFFF; // 8 bits
    if (func_num > 0xFF) {
        LOG_ERROR(Kernel_SVC,"unknown svc=0x%02X", func_num);
        return nullptr;
    }
    return &g_module_db[0].func_table[func_num];
}

void CallSVC(u32 opcode) {
    const FunctionDef *info = GetSVCInfo(opcode);

    if (!info) {
        return;
    }
    if (info->func) {
        info->func();
    } else {
        LOG_ERROR(Kernel_SVC, "unimplemented SVC function %s(..)", info->name.c_str());
    }
}

void Reschedule(const char *reason) {
    _dbg_assert_msg_(Kernel, reason != 0 && strlen(reason) < 256, "Reschedule: Invalid or too long reason.");
    Core::g_app_core->PrepareReschedule();
    g_reschedule = true;
}

void RegisterModule(std::string name, int num_functions, const FunctionDef* func_table) {
    ModuleDef module = {name, num_functions, func_table};
    g_module_db.push_back(module);
}

void RegisterAllModules() {
    SVC::Register();
}

void Init() {
    Service::Init();
    Service::FS::ArchiveInit();
    Service::CFG::CFGInit();

    RegisterAllModules();

    LOG_DEBUG(Kernel, "initialized OK");
}

void Shutdown() {
    Service::CFG::CFGShutdown();
    Service::FS::ArchiveShutdown();
    Service::Shutdown();

    g_module_db.clear();

    LOG_DEBUG(Kernel, "shutdown OK");
}

} // namespace
