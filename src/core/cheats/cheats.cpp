// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <functional>
#include <fmt/format.h>
#include "core/cheats/cheats.h"
#include "core/cheats/gateway_cheat.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/process.h"

namespace Cheats {

constexpr u64 run_interval_ticks = BASE_CLOCK_RATE_ARM11 / 60;

CheatEngine::CheatEngine(Core::System& system_) : system(system_) {
    LoadCheatFile();
    event = system.CoreTiming().RegisterEvent(
        "CheatCore::run_event",
        [this](u64 thread_id, s64 cycle_late) { RunCallback(thread_id, cycle_late); });
    system.CoreTiming().ScheduleEvent(run_interval_ticks, event);
}

CheatEngine::~CheatEngine() {
    system.CoreTiming().UnscheduleEvent(event, 0);
}

const std::vector<std::unique_ptr<CheatBase>>& CheatEngine::GetCheats() const {
    return cheats_list;
}

void CheatEngine::LoadCheatFile() {
    const std::string cheat_dir = FileUtil::GetUserPath(FileUtil::UserPath::CheatsDir);
    const std::string filepath = fmt::format(
        "{}{:016X}.txt", cheat_dir, system.Kernel().GetCurrentProcess()->codeset->program_id);

    if (!FileUtil::IsDirectory(cheat_dir)) {
        FileUtil::CreateDir(cheat_dir);
    }

    if (!FileUtil::Exists(filepath))
        return;

    auto gateway_cheats = GatewayCheat::LoadFile(filepath);
    std::move(gateway_cheats.begin(), gateway_cheats.end(), std::back_inserter(cheats_list));
}

void CheatEngine::RunCallback([[maybe_unused]] u64 userdata, int cycles_late) {
    for (auto& cheat : cheats_list) {
        if (cheat->IsEnabled()) {
            cheat->Execute(system);
        }
    }
    system.CoreTiming().ScheduleEvent(run_interval_ticks - cycles_late, event);
}

} // namespace Cheats
