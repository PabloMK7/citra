// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "common/common_types.h"

namespace DynamicLibrary {

class DynamicLibrary {
public:
    explicit DynamicLibrary(std::string_view name, int major = -1, int minor = -1);
    ~DynamicLibrary();

    bool IsLoaded() {
        return handle != nullptr;
    }

    std::string_view GetLoadError() {
        return load_error;
    }

    template <typename T>
    T GetSymbol(std::string_view name) {
        return reinterpret_cast<T>(GetRawSymbol(name));
    }

    static std::string GetLibraryName(std::string_view name, int major = -1, int minor = -1);

private:
    void* GetRawSymbol(std::string_view name);

    void* handle;
    std::string load_error;
};

} // namespace DynamicLibrary
