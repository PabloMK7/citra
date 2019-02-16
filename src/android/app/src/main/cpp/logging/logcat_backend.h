// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/logging/backend.h"

namespace Log {
class LogcatBackend : public Backend {
public:
    static const char* Name() {
        return "Logcat";
    }

    const char* GetName() const override {
        return Name();
    }

    void Write(const Entry& entry) override;
};
} // namespace Log
