// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <android/log.h>
#include "common/assert.h"
#include "common/logging/text_formatter.h"
#include "logcat_backend.h"

namespace Log {
void LogcatBackend::Write(const Entry& entry) {
    android_LogPriority priority;
    switch (entry.log_level) {
    case Level::Trace:
        priority = ANDROID_LOG_VERBOSE;
        break;
    case Level::Debug:
        priority = ANDROID_LOG_DEBUG;
        break;
    case Level::Info:
        priority = ANDROID_LOG_INFO;
        break;
    case Level::Warning:
        priority = ANDROID_LOG_WARN;
        break;
    case Level::Error:
        priority = ANDROID_LOG_ERROR;
        break;
    case Level::Critical:
        priority = ANDROID_LOG_FATAL;
        break;
    case Level::Count:
        UNREACHABLE();
    }

    __android_log_print(priority, "citra", "%s\n", FormatLogMessage(entry).c_str());
}
} // namespace Log