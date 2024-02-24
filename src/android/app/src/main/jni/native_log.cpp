// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <common/logging/log.h>
#include <jni.h>
#include "android_common/android_common.h"

extern "C" {

void Java_org_citra_citra_1emu_utils_Log_debug(JNIEnv* env, jobject obj, jstring jmessage) {
    LOG_DEBUG(Frontend, "{}", GetJString(env, jmessage));
}

void Java_org_citra_citra_1emu_utils_Log_warning(JNIEnv* env, jobject obj, jstring jmessage) {
    LOG_WARNING(Frontend, "{}", GetJString(env, jmessage));
}

void Java_org_citra_citra_1emu_utils_Log_info(JNIEnv* env, jobject obj, jstring jmessage) {
    LOG_INFO(Frontend, "{}", GetJString(env, jmessage));
}

void Java_org_citra_citra_1emu_utils_Log_error(JNIEnv* env, jobject obj, jstring jmessage) {
    LOG_ERROR(Frontend, "{}", GetJString(env, jmessage));
}

void Java_org_citra_citra_1emu_utils_Log_critical(JNIEnv* env, jobject obj, jstring jmessage) {
    LOG_CRITICAL(Frontend, "{}", GetJString(env, jmessage));
}

} // extern "C"
