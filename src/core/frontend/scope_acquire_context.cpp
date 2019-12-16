// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/frontend/emu_window.h"
#include "core/frontend/scope_acquire_context.h"

namespace Frontend {

ScopeAcquireContext::ScopeAcquireContext(Frontend::GraphicsContext& context) : context{context} {
    context.MakeCurrent();
}
ScopeAcquireContext::~ScopeAcquireContext() {
    context.DoneCurrent();
}

} // namespace Frontend