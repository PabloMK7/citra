// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <dynarmic/interface/exclusive_monitor.h>

#include "common/common_types.h"
#include "core/arm/dynarmic/arm_dynarmic.h"
#include "core/arm/exclusive_monitor.h"

namespace Memory {
class MemorySystem;
}

namespace Core {

class DynarmicExclusiveMonitor final : public ExclusiveMonitor {
public:
    explicit DynarmicExclusiveMonitor(Memory::MemorySystem& memory_, std::size_t core_count_);
    ~DynarmicExclusiveMonitor() override;

    u8 ExclusiveRead8(std::size_t core_index, VAddr addr) override;
    u16 ExclusiveRead16(std::size_t core_index, VAddr addr) override;
    u32 ExclusiveRead32(std::size_t core_index, VAddr addr) override;
    u64 ExclusiveRead64(std::size_t core_index, VAddr addr) override;
    void ClearExclusive(std::size_t core_index) override;

    bool ExclusiveWrite8(std::size_t core_index, VAddr vaddr, u8 value) override;
    bool ExclusiveWrite16(std::size_t core_index, VAddr vaddr, u16 value) override;
    bool ExclusiveWrite32(std::size_t core_index, VAddr vaddr, u32 value) override;
    bool ExclusiveWrite64(std::size_t core_index, VAddr vaddr, u64 value) override;

private:
    friend class Core::ARM_Dynarmic;
    Dynarmic::ExclusiveMonitor monitor;
    Memory::MemorySystem& memory;
};

} // namespace Core
