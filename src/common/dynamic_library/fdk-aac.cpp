// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "common/dynamic_library/dynamic_library.h"
#include "common/dynamic_library/fdk-aac.h"
#include "common/logging/log.h"

namespace DynamicLibrary::FdkAac {

aacDecoder_GetLibInfo_func aacDecoder_GetLibInfo;
aacDecoder_Open_func aacDecoder_Open;
aacDecoder_Close_func aacDecoder_Close;
aacDecoder_SetParam_func aacDecoder_SetParam;
aacDecoder_GetStreamInfo_func aacDecoder_GetStreamInfo;
aacDecoder_DecodeFrame_func aacDecoder_DecodeFrame;
aacDecoder_Fill_func aacDecoder_Fill;

static std::unique_ptr<Common::DynamicLibrary> fdk_aac;

#define LOAD_SYMBOL(library, name)                                                                 \
    any_failed = any_failed || (name = library->GetSymbol<name##_func>(#name)) == nullptr

bool LoadFdkAac() {
    if (fdk_aac) {
        return true;
    }

    fdk_aac = std::make_unique<Common::DynamicLibrary>("fdk-aac", 2);
    if (!fdk_aac->IsLoaded()) {
        LOG_WARNING(Common, "Could not dynamically load libfdk-aac: {}", fdk_aac->GetLoadError());
        fdk_aac.reset();
        return false;
    }

    auto any_failed = false;
    LOAD_SYMBOL(fdk_aac, aacDecoder_GetLibInfo);
    LOAD_SYMBOL(fdk_aac, aacDecoder_Open);
    LOAD_SYMBOL(fdk_aac, aacDecoder_Close);
    LOAD_SYMBOL(fdk_aac, aacDecoder_SetParam);
    LOAD_SYMBOL(fdk_aac, aacDecoder_GetStreamInfo);
    LOAD_SYMBOL(fdk_aac, aacDecoder_DecodeFrame);
    LOAD_SYMBOL(fdk_aac, aacDecoder_Fill);

    if (any_failed) {
        LOG_WARNING(Common, "Could not find all required functions in libfdk-aac.");
        fdk_aac.reset();
        return false;
    }

    LOG_INFO(Common, "Successfully loaded libfdk-aac.");
    return true;
}

} // namespace DynamicLibrary::FdkAac
