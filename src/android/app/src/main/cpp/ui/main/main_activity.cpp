// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/file_util.h"
#include "native_interface.h"

namespace MainActivity {
extern "C" {
JNICALL void Java_org_citra_1emu_citra_ui_main_MainActivity_initUserPath(JNIEnv* env, jclass type,
                                                                         jstring path) {
    FileUtil::SetUserPath(CitraJNI::GetJString(env, path));
}
};
}; // namespace MainActivity
