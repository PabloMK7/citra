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

JNIEXPORT jobjectArray JNICALL
Java_org_citra_citra_1emu_features_cheats_model_CheatEngine_getCheats(JNIEnv* env, jclass) {
    auto cheats = Core::System::GetInstance().CheatEngine().GetCheats();

    const jobjectArray array =
        env->NewObjectArray(static_cast<jsize>(cheats.size()), IDCache::GetCheatClass(), nullptr);

    jsize i = 0;
    for (auto& cheat : cheats)
        env->SetObjectArrayElement(array, i++, CheatToJava(env, std::move(cheat)));

    return array;
}

JNIEXPORT void JNICALL Java_org_citra_citra_1emu_features_cheats_model_CheatEngine_addCheat(
    JNIEnv* env, jclass, jobject j_cheat) {
    Core::System::GetInstance().CheatEngine().AddCheat(*CheatFromJava(env, j_cheat));
}

JNIEXPORT void JNICALL Java_org_citra_citra_1emu_features_cheats_model_CheatEngine_removeCheat(
    JNIEnv* env, jclass, jint index) {
    Core::System::GetInstance().CheatEngine().RemoveCheat(index);
}

JNIEXPORT void JNICALL Java_org_citra_citra_1emu_features_cheats_model_CheatEngine_updateCheat(
    JNIEnv* env, jclass, jint index, jobject j_new_cheat) {
    Core::System::GetInstance().CheatEngine().UpdateCheat(index, *CheatFromJava(env, j_new_cheat));
}

JNIEXPORT void JNICALL
Java_org_citra_citra_1emu_features_cheats_model_CheatEngine_saveCheatFile(JNIEnv* env, jclass) {
    Core::System::GetInstance().CheatEngine().SaveCheatFile();
}
}
