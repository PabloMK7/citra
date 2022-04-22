// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <type_traits>
#include <jni.h>
#include "video_core/rasterizer_interface.h"

namespace IDCache {

JNIEnv* GetEnvForThread();
jclass GetNativeLibraryClass();
jclass GetCoreErrorClass();
jclass GetSavestateInfoClass();
jclass GetDiskCacheProgressClass();
jclass GetDiskCacheLoadCallbackStageClass();
jmethodID GetOnCoreError();
jmethodID GetDisplayAlertMsg();
jmethodID GetDisplayAlertPrompt();
jmethodID GetAlertPromptButton();
jmethodID GetIsPortraitMode();
jmethodID GetLandscapeScreenLayout();
jmethodID GetExitEmulationActivity();
jmethodID GetRequestCameraPermission();
jmethodID GetRequestMicPermission();
jmethodID GetDiskCacheLoadProgress();

jobject GetJavaLoadCallbackStage(VideoCore::LoadCallbackStage stage);

} // namespace IDCache

template <typename T = jobject>
using SharedGlobalRef = std::shared_ptr<std::remove_pointer_t<T>>;

struct SharedGlobalRefDeleter {
    void operator()(jobject ptr) {
        JNIEnv* env = IDCache::GetEnvForThread();
        env->DeleteGlobalRef(ptr);
    }
};

template <typename T = jobject>
SharedGlobalRef<T> NewSharedGlobalRef(T object) {
    JNIEnv* env = IDCache::GetEnvForThread();
    auto* global_ref = reinterpret_cast<T>(env->NewGlobalRef(object));
    return SharedGlobalRef<T>(global_ref, SharedGlobalRefDeleter());
}
