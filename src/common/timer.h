// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include <string>
#include "common/common_types.h"

namespace Common {
class Timer {
public:
    Timer();

    void Start();
    void Stop();
    void Update();

    // The time difference is always returned in milliseconds, regardless of alternative internal
    // representation
    std::chrono::milliseconds GetTimeDifference();
    void AddTimeDifference();

    static std::chrono::seconds GetTimeSinceJan1970();
    static std::chrono::seconds GetLocalTimeSinceJan1970();
    static double GetDoubleTime();

    static std::string GetTimeFormatted();
    std::string GetTimeElapsedFormatted() const;
    std::chrono::milliseconds GetTimeElapsed();

    static std::chrono::milliseconds GetTimeMs();

private:
    std::chrono::milliseconds m_LastTime;
    std::chrono::milliseconds m_StartTime;
    bool m_Running;
};

} // Namespace Common
