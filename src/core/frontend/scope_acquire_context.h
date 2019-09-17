// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Frontend {

class GraphicsContext;

/// Helper class to acquire/release window context within a given scope
class ScopeAcquireContext : NonCopyable {
public:
    explicit ScopeAcquireContext(Frontend::GraphicsContext& context);
    ~ScopeAcquireContext();

private:
    Frontend::GraphicsContext& context;
};

} // namespace Frontend