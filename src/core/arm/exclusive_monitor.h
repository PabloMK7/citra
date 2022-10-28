// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "common/common_types.h"

namespace Memory {
class MemorySystem;
}

namespace Core {

class ExclusiveMonitor {
public:
    virtual ~ExclusiveMonitor();

    virtual u8 ExclusiveRead8(std::size_t core_index, VAddr addr) = 0;
    virtual u16 ExclusiveRead16(std::size_t core_index, VAddr addr) = 0;
    virtual u32 ExclusiveRead32(std::size_t core_index, VAddr addr) = 0;
    virtual u64 ExclusiveRead64(std::size_t core_index, VAddr addr) = 0;
    virtual void ClearExclusive(std::size_t core_index) = 0;

    virtual bool ExclusiveWrite8(std::size_t core_index, VAddr vaddr, u8 value) = 0;
    virtual bool ExclusiveWrite16(std::size_t core_index, VAddr vaddr, u16 value) = 0;
    virtual bool ExclusiveWrite32(std::size_t core_index, VAddr vaddr, u32 value) = 0;
    virtual bool ExclusiveWrite64(std::size_t core_index, VAddr vaddr, u64 value) = 0;
};

std::unique_ptr<Core::ExclusiveMonitor> MakeExclusiveMonitor(Memory::MemorySystem& memory,
                                                             std::size_t num_cores);

} // namespace Core
