// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/renderer_software/renderer_software.h"

namespace VideoCore {

RendererSoftware::RendererSoftware(Core::System& system, Frontend::EmuWindow& window)
    : VideoCore::RendererBase{system, window, nullptr},
      rasterizer{std::make_unique<RasterizerSoftware>()} {}

RendererSoftware::~RendererSoftware() = default;

void RendererSoftware::SwapBuffers() {
    EndFrame();
}

} // namespace VideoCore
