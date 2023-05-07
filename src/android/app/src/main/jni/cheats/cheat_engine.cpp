// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>
#include <vector>

#include <jni.h>

#include "core/cheats/cheat_base.h"
#include "core/cheats/cheats.h"
#include "core/core.h"
#include "jni/cheats/cheat.h"
#include "jni/id_cache.h"

extern "C" {

static Cheats::CheatEngine* GetPointer(JNIEnv* env, jobject obj) {
    return reinterpret_cast<Cheats::CheatEngine*>(
        env->GetLongField(obj, IDCache::GetCheatEnginePointer()));
}

JNIEXPORT jlong JNICALL Java_org_citra_citra_1emu_features_cheats_model_CheatEngine_initialize(
    JNIEnv* env, jclass, jlong title_id) {
    return reinterpret_cast<jlong>(new Cheats::CheatEngine(title_id, Core::System::GetInstance()));
}

JNIEXPORT void JNICALL
Java_org_citra_citra_1emu_features_cheats_model_CheatEngine_finalize(JNIEnv* env, jobject obj) {
    delete GetPointer(env, obj);
}

JNIEXPORT jobjectArray JNICALL
Java_org_citra_citra_1emu_features_cheats_model_CheatEngine_getCheats(JNIEnv* env, jobject obj) {
    auto cheats = GetPointer(env, obj)->GetCheats();

    const jobjectArray array =
        env->NewObjectArray(static_cast<jsize>(cheats.size()), IDCache::GetCheatClass(), nullptr);

    jsize i = 0;
    for (auto& cheat : cheats)
        env->SetObjectArrayElement(array, i++, CheatToJava(env, std::move(cheat)));

    return array;
}

JNIEXPORT void JNICALL Java_org_citra_citra_1emu_features_cheats_model_CheatEngine_addCheat(
    JNIEnv* env, jobject obj, jobject j_cheat) {
    GetPointer(env, obj)->AddCheat(*CheatFromJava(env, j_cheat));
}

JNIEXPORT void JNICALL Java_org_citra_citra_1emu_features_cheats_model_CheatEngine_removeCheat(
    JNIEnv* env, jobject obj, jint index) {
    GetPointer(env, obj)->RemoveCheat(index);
}

JNIEXPORT void JNICALL Java_org_citra_citra_1emu_features_cheats_model_CheatEngine_updateCheat(
    JNIEnv* env, jobject obj, jint index, jobject j_new_cheat) {
    GetPointer(env, obj)->UpdateCheat(index, *CheatFromJava(env, j_new_cheat));
}

JNIEXPORT void JNICALL Java_org_citra_citra_1emu_features_cheats_model_CheatEngine_saveCheatFile(
    JNIEnv* env, jobject obj) {
    GetPointer(env, obj)->SaveCheatFile();
}
}
