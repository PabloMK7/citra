// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <jni.h>

namespace CitraJNI {
extern "C" {
jint JNI_OnLoad(JavaVM* vm, void* reserved);
}

std::string GetJString(JNIEnv* env, jstring jstr);
} // namespace CitraJNI
