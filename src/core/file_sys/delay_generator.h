// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include "common/common_types.h"

namespace FileSys {

class DelayGenerator {
public:
    virtual ~DelayGenerator();
    virtual u64 GetReadDelayNs(std::size_t length) = 0;

    // TODO (B3N30): Add getter for all other file/directory io operations
};

class DefaultDelayGenerator : public DelayGenerator {
public:
    u64 GetReadDelayNs(std::size_t length) override;
};

} // namespace FileSys
