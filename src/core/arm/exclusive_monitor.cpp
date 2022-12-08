// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#if defined(ARCHITECTURE_x86_64) || defined(ARCHITECTURE_arm64)
#include "core/arm/dynarmic/arm_exclusive_monitor.h"
#endif
#include "common/settings.h"
#include "core/arm/exclusive_monitor.h"
#include "core/memory.h"

namespace Core {

ExclusiveMonitor::~ExclusiveMonitor() = default;

std::unique_ptr<Core::ExclusiveMonitor> MakeExclusiveMonitor(Memory::MemorySystem& memory,
                                                             std::size_t num_cores) {
#if defined(ARCHITECTURE_x86_64) || defined(ARCHITECTURE_arm64)
    if (Settings::values.use_cpu_jit) {
        return std::make_unique<Core::DynarmicExclusiveMonitor>(memory, num_cores);
    }
#endif
    // TODO(merry): Passthrough exclusive monitor
    return nullptr;
}

} // namespace Core
