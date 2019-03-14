// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "native_interface.h"

namespace CitraJNI {
jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    return JNI_VERSION_1_6;
}

std::string GetJString(JNIEnv* env, jstring jstr) {
    std::string result = "";
    if (!jstr)
        return result;

    const char* s = env->GetStringUTFChars(jstr, nullptr);
    result = s;
    env->ReleaseStringUTFChars(jstr, s);
    return result;
}
} // namespace CitraJNI
