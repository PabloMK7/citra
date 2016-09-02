// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "common/logging/log.h"

#include "core/arm/arm_interface.h"
#include "core/arm/dynarmic/arm_dynarmic.h"
#include "core/arm/dyncom/arm_dyncom.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/gdbstub/gdbstub.h"
#include "core/hle/hle.h"
#include "core/hle/kernel/thread.h"
#include "core/hw/hw.h"
#include "core/settings.h"

namespace Core {

std::unique_ptr<ARM_Interface> g_app_core; ///< ARM11 application core
std::unique_ptr<ARM_Interface> g_sys_core; ///< ARM11 system (OS) core

/// Run the core CPU loop
void RunLoop(int tight_loop) {
    if (GDBStub::g_server_enabled) {
        GDBStub::HandlePacket();

        // If the loop is halted and we want to step, use a tiny (1) number of instructions to execute.
        // Otherwise get out of the loop function.
        if (GDBStub::GetCpuHaltFlag()) {
            if (GDBStub::GetCpuStepFlag()) {
                GDBStub::SetCpuStepFlag(false);
                tight_loop = 1;
            } else {
                return;
            }
        }
    }

    // If we don't have a currently active thread then don't execute instructions,
    // instead advance to the next event and try to yield to the next thread
    if (Kernel::GetCurrentThread() == nullptr) {
        LOG_TRACE(Core_ARM11, "Idling");
        CoreTiming::Idle();
        CoreTiming::Advance();
        HLE::Reschedule(__func__);
    } else {
        g_app_core->Run(tight_loop);
    }

    HW::Update();
    if (HLE::IsReschedulePending()) {
        Kernel::Reschedule();
    }
}

/// Step the CPU one instruction
void SingleStep() {
    RunLoop(1);
}

/// Halt the core
void Halt(const char *msg) {
    // TODO(ShizZy): ImplementMe
}

/// Kill the core
void Stop() {
    // TODO(ShizZy): ImplementMe
}

/// Initialize the core
void Init() {
    if (Settings::values.use_cpu_jit) {
        g_sys_core = std::make_unique<ARM_Dynarmic>(USER32MODE);
        g_app_core = std::make_unique<ARM_Dynarmic>(USER32MODE);
    } else {
        g_sys_core = std::make_unique<ARM_DynCom>(USER32MODE);
        g_app_core = std::make_unique<ARM_DynCom>(USER32MODE);
    }

    LOG_DEBUG(Core, "Initialized OK");
}

void Shutdown() {
    g_app_core.reset();
    g_sys_core.reset();

    LOG_DEBUG(Core, "Shutdown OK");
}

} // namespace
