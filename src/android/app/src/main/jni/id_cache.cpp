// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_paths.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/logging/log.h"
#include "core/settings.h"
#include "jni/applets/mii_selector.h"
#include "jni/applets/swkbd.h"
#include "jni/camera/still_image_camera.h"
#include "jni/id_cache.h"

#include <jni.h>

static constexpr jint JNI_VERSION = JNI_VERSION_1_6;

static JavaVM* s_java_vm;

static jclass s_native_library_class;
static jclass s_core_error_class;
static jclass s_savestate_info_class;
static jclass s_disk_cache_progress_class;
static jclass s_load_callback_stage_class;
static jmethodID s_on_core_error;
static jmethodID s_display_alert_msg;
static jmethodID s_display_alert_prompt;
static jmethodID s_alert_prompt_button;
static jmethodID s_is_portrait_mode;
static jmethodID s_landscape_screen_layout;
static jmethodID s_exit_emulation_activity;
static jmethodID s_request_camera_permission;
static jmethodID s_request_mic_permission;
static jmethodID s_disk_cache_load_progress;

static std::unordered_map<VideoCore::LoadCallbackStage, jobject> s_java_load_callback_stages;

namespace IDCache {

JNIEnv* GetEnvForThread() {
    thread_local static struct OwnedEnv {
        OwnedEnv() {
            status = s_java_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
            if (status == JNI_EDETACHED)
                s_java_vm->AttachCurrentThread(&env, nullptr);
        }

        ~OwnedEnv() {
            if (status == JNI_EDETACHED)
                s_java_vm->DetachCurrentThread();
        }

        int status;
        JNIEnv* env = nullptr;
    } owned;
    return owned.env;
}

jclass GetNativeLibraryClass() {
    return s_native_library_class;
}

jclass GetCoreErrorClass() {
    return s_core_error_class;
}

jclass GetSavestateInfoClass() {
    return s_savestate_info_class;
}

jclass GetDiskCacheProgressClass() {
    return s_disk_cache_progress_class;
}

jclass GetDiskCacheLoadCallbackStageClass() {
    return s_load_callback_stage_class;
}

jmethodID GetOnCoreError() {
    return s_on_core_error;
}

jmethodID GetDisplayAlertMsg() {
    return s_display_alert_msg;
}

jmethodID GetDisplayAlertPrompt() {
    return s_display_alert_prompt;
}

jmethodID GetAlertPromptButton() {
    return s_alert_prompt_button;
}

jmethodID GetIsPortraitMode() {
    return s_is_portrait_mode;
}

jmethodID GetLandscapeScreenLayout() {
    return s_landscape_screen_layout;
}

jmethodID GetExitEmulationActivity() {
    return s_exit_emulation_activity;
}

jmethodID GetRequestCameraPermission() {
    return s_request_camera_permission;
}

jmethodID GetRequestMicPermission() {
    return s_request_mic_permission;
}

jmethodID GetDiskCacheLoadProgress() {
    return s_disk_cache_load_progress;
}

jobject GetJavaLoadCallbackStage(VideoCore::LoadCallbackStage stage) {
    const auto it = s_java_load_callback_stages.find(stage);
    ASSERT_MSG(it != s_java_load_callback_stages.end(), "Invalid LoadCallbackStage: {}", stage);

    return it->second;
}

} // namespace IDCache

#ifdef __cplusplus
extern "C" {
#endif

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    s_java_vm = vm;

    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION) != JNI_OK)
        return JNI_ERR;

    // Initialize Logger
    Log::Filter log_filter;
    log_filter.ParseFilterString(Settings::values.log_filter);
    Log::SetGlobalFilter(log_filter);
    Log::AddBackend(std::make_unique<Log::LogcatBackend>());
    FileUtil::CreateFullPath(FileUtil::GetUserPath(FileUtil::UserPath::LogDir));
    Log::AddBackend(std::make_unique<Log::FileBackend>(
        FileUtil::GetUserPath(FileUtil::UserPath::LogDir) + LOG_FILE));
    LOG_INFO(Frontend, "Logging backend initialised");

    // Initialize Java classes
    const jclass native_library_class = env->FindClass("org/citra/citra_emu/NativeLibrary");
    s_native_library_class = reinterpret_cast<jclass>(env->NewGlobalRef(native_library_class));
    s_savestate_info_class = reinterpret_cast<jclass>(
        env->NewGlobalRef(env->FindClass("org/citra/citra_emu/NativeLibrary$SavestateInfo")));
    s_core_error_class = reinterpret_cast<jclass>(
        env->NewGlobalRef(env->FindClass("org/citra/citra_emu/NativeLibrary$CoreError")));
    s_disk_cache_progress_class = reinterpret_cast<jclass>(env->NewGlobalRef(
        env->FindClass("org/citra/citra_emu/disk_shader_cache/DiskShaderCacheProgress")));
    s_load_callback_stage_class = reinterpret_cast<jclass>(env->NewGlobalRef(env->FindClass(
        "org/citra/citra_emu/disk_shader_cache/DiskShaderCacheProgress$LoadCallbackStage")));

    // Initialize Java methods
    s_on_core_error = env->GetStaticMethodID(
        s_native_library_class, "OnCoreError",
        "(Lorg/citra/citra_emu/NativeLibrary$CoreError;Ljava/lang/String;)Z");
    s_display_alert_msg = env->GetStaticMethodID(s_native_library_class, "displayAlertMsg",
                                                 "(Ljava/lang/String;Ljava/lang/String;Z)Z");
    s_display_alert_prompt =
        env->GetStaticMethodID(s_native_library_class, "displayAlertPrompt",
                               "(Ljava/lang/String;Ljava/lang/String;I)Ljava/lang/String;");
    s_alert_prompt_button =
        env->GetStaticMethodID(s_native_library_class, "alertPromptButton", "()I");
    s_is_portrait_mode = env->GetStaticMethodID(s_native_library_class, "isPortraitMode", "()Z");
    s_landscape_screen_layout =
        env->GetStaticMethodID(s_native_library_class, "landscapeScreenLayout", "()I");
    s_exit_emulation_activity =
        env->GetStaticMethodID(s_native_library_class, "exitEmulationActivity", "(I)V");
    s_request_camera_permission =
        env->GetStaticMethodID(s_native_library_class, "RequestCameraPermission", "()Z");
    s_request_mic_permission =
        env->GetStaticMethodID(s_native_library_class, "RequestMicPermission", "()Z");
    s_disk_cache_load_progress = env->GetStaticMethodID(
        s_disk_cache_progress_class, "loadProgress",
        "(Lorg/citra/citra_emu/disk_shader_cache/DiskShaderCacheProgress$LoadCallbackStage;II)V");

    // Initialize LoadCallbackStage map
    const auto to_java_load_callback_stage = [env](const std::string& stage) {
        jclass load_callback_stage_class = IDCache::GetDiskCacheLoadCallbackStageClass();
        return env->NewGlobalRef(env->GetStaticObjectField(
            load_callback_stage_class,
            env->GetStaticFieldID(load_callback_stage_class, stage.c_str(),
                                  "Lorg/citra/citra_emu/disk_shader_cache/"
                                  "DiskShaderCacheProgress$LoadCallbackStage;")));
    };

    s_java_load_callback_stages.emplace(VideoCore::LoadCallbackStage::Prepare,
                                        to_java_load_callback_stage("Prepare"));
    s_java_load_callback_stages.emplace(VideoCore::LoadCallbackStage::Decompile,
                                        to_java_load_callback_stage("Decompile"));
    s_java_load_callback_stages.emplace(VideoCore::LoadCallbackStage::Build,
                                        to_java_load_callback_stage("Build"));
    s_java_load_callback_stages.emplace(VideoCore::LoadCallbackStage::Complete,
                                        to_java_load_callback_stage("Complete"));

    MiiSelector::InitJNI(env);
    SoftwareKeyboard::InitJNI(env);
    Camera::StillImage::InitJNI(env);

    return JNI_VERSION;
}

void JNI_OnUnload(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION) != JNI_OK) {
        return;
    }

    env->DeleteGlobalRef(s_native_library_class);
    env->DeleteGlobalRef(s_savestate_info_class);
    env->DeleteGlobalRef(s_core_error_class);
    env->DeleteGlobalRef(s_disk_cache_progress_class);
    env->DeleteGlobalRef(s_load_callback_stage_class);

    for (auto& [key, object] : s_java_load_callback_stages) {
        env->DeleteGlobalRef(object);
    }

    MiiSelector::CleanupJNI(env);
    SoftwareKeyboard::CleanupJNI(env);
    Camera::StillImage::CleanupJNI(env);
}

#ifdef __cplusplus
}
#endif
