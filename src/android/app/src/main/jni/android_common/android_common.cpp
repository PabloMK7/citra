// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "jni/android_common/android_common.h"

#include <string>

#include <jni.h>

std::string GetJString(JNIEnv *env, jstring jstr) {
    if (!jstr) {
        return {};
    }

    const char *s = env->GetStringUTFChars(jstr, nullptr);
    std::string result = s;
    env->ReleaseStringUTFChars(jstr, s);
    return result;
}

jstring ToJString(JNIEnv* env, const std::string& str) {
    return env->NewStringUTF(str.c_str());
}
