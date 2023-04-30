// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <shared_mutex>
#include <vector>
#include "common/common_types.h"

namespace Core {
class System;
struct TimingEventType;
} // namespace Core

namespace CoreTiming {
struct EventType;
}

namespace Cheats {

class CheatBase;

class CheatEngine {
public:
    explicit CheatEngine(u64 title_id_, Core::System& system);
    ~CheatEngine();
    void Connect();
    std::vector<std::shared_ptr<CheatBase>> GetCheats() const;
    void AddCheat(const std::shared_ptr<CheatBase>& cheat);
    void RemoveCheat(std::size_t index);
    void UpdateCheat(std::size_t index, const std::shared_ptr<CheatBase>& new_cheat);
    void SaveCheatFile() const;

private:
    void LoadCheatFile();
    void RunCallback(std::uintptr_t user_data, s64 cycles_late);
    std::vector<std::shared_ptr<CheatBase>> cheats_list;
    mutable std::shared_mutex cheats_list_mutex;
    Core::TimingEventType* event;
    Core::System& system;
    u64 title_id;
};
} // namespace Cheats
