// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <jni.h>
#include "common/logging/log.h"
#include "jni/id_cache.h"
#include "jni/mic.h"

#ifdef HAVE_CUBEB
#include "audio_core/cubeb_input.h"
#endif

namespace Mic {

AndroidFactory::~AndroidFactory() = default;

std::unique_ptr<Frontend::Mic::Interface> AndroidFactory::Create(std::string mic_device_name) {
#ifdef HAVE_CUBEB
    if (!permission_granted) {
        JNIEnv* env = IDCache::GetEnvForThread();
        permission_granted = env->CallStaticBooleanMethod(IDCache::GetNativeLibraryClass(),
                                                          IDCache::GetRequestMicPermission());
    }

    if (permission_granted) {
        return std::make_unique<AudioCore::CubebInput>(std::move(mic_device_name));
    } else {
        LOG_WARNING(Frontend, "Mic permissions denied");
        return std::make_unique<Frontend::Mic::NullMic>();
    }
#else
    LOG_WARNING(Frontend, "No cubeb support");
    return std::make_unique<Frontend::Mic::NullMic>();
#endif
}

} // namespace Mic
