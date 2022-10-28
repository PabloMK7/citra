// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/arm/dynarmic/arm_exclusive_monitor.h"
#include "core/memory.h"

namespace Core {

DynarmicExclusiveMonitor::DynarmicExclusiveMonitor(Memory::MemorySystem& memory_,
                                                   std::size_t core_count_)
    : monitor{core_count_}, memory{memory_} {}

DynarmicExclusiveMonitor::~DynarmicExclusiveMonitor() = default;

u8 DynarmicExclusiveMonitor::ExclusiveRead8(std::size_t core_index, VAddr addr) {
    return monitor.ReadAndMark<u8>(core_index, addr, [&]() -> u8 { return memory.Read8(addr); });
}

u16 DynarmicExclusiveMonitor::ExclusiveRead16(std::size_t core_index, VAddr addr) {
    return monitor.ReadAndMark<u16>(core_index, addr, [&]() -> u16 { return memory.Read16(addr); });
}

u32 DynarmicExclusiveMonitor::ExclusiveRead32(std::size_t core_index, VAddr addr) {
    return monitor.ReadAndMark<u32>(core_index, addr, [&]() -> u32 { return memory.Read32(addr); });
}

u64 DynarmicExclusiveMonitor::ExclusiveRead64(std::size_t core_index, VAddr addr) {
    return monitor.ReadAndMark<u64>(core_index, addr, [&]() -> u64 { return memory.Read64(addr); });
}

void DynarmicExclusiveMonitor::ClearExclusive(std::size_t core_index) {
    monitor.ClearProcessor(core_index);
}

bool DynarmicExclusiveMonitor::ExclusiveWrite8(std::size_t core_index, VAddr vaddr, u8 value) {
    return monitor.DoExclusiveOperation<u8>(core_index, vaddr, [&](u8 expected) -> bool {
        return memory.WriteExclusive8(vaddr, value, expected);
    });
}

bool DynarmicExclusiveMonitor::ExclusiveWrite16(std::size_t core_index, VAddr vaddr, u16 value) {
    return monitor.DoExclusiveOperation<u16>(core_index, vaddr, [&](u16 expected) -> bool {
        return memory.WriteExclusive16(vaddr, value, expected);
    });
}

bool DynarmicExclusiveMonitor::ExclusiveWrite32(std::size_t core_index, VAddr vaddr, u32 value) {
    return monitor.DoExclusiveOperation<u32>(core_index, vaddr, [&](u32 expected) -> bool {
        return memory.WriteExclusive32(vaddr, value, expected);
    });
}

bool DynarmicExclusiveMonitor::ExclusiveWrite64(std::size_t core_index, VAddr vaddr, u64 value) {
    return monitor.DoExclusiveOperation<u64>(core_index, vaddr, [&](u64 expected) -> bool {
        return memory.WriteExclusive64(vaddr, value, expected);
    });
}

} // namespace Core
