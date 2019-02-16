// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/logging/filter.h"
#include "common/logging/log.h"
#include "core/settings.h"
#include "logging/logcat_backend.h"
#include "native_interface.h"

namespace MainActivity {
extern "C" {
JNICALL void Java_org_citra_1emu_citra_ui_main_MainActivity_initUserPath(JNIEnv* env, jclass type,
                                                                         jstring path) {
    FileUtil::SetUserPath(CitraJNI::GetJString(env, path) + '/');
}

JNICALL void Java_org_citra_1emu_citra_ui_main_MainActivity_initLogging(JNIEnv* env, jclass type) {
    Log::Filter log_filter(Log::Level::Debug);
    log_filter.ParseFilterString(Settings::values.log_filter);
    Log::SetGlobalFilter(log_filter);

    const std::string& log_dir = FileUtil::GetUserPath(FileUtil::UserPath::LogDir);
    FileUtil::CreateFullPath(log_dir);
    Log::AddBackend(std::make_unique<Log::FileBackend>(log_dir + LOG_FILE));
    Log::AddBackend(std::make_unique<Log::LogcatBackend>());
}
};
}; // namespace MainActivity
