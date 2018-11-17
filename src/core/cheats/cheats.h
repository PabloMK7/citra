// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
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
    explicit CheatEngine(Core::System& system);
    ~CheatEngine();
    const std::vector<std::unique_ptr<CheatBase>>& GetCheats() const;

private:
    void LoadCheatFile();
    void RunCallback(u64 userdata, int cycles_late);
    std::vector<std::unique_ptr<CheatBase>> cheats_list;
    Core::TimingEventType* event;
    Core::System& system;
};
} // namespace Cheats
