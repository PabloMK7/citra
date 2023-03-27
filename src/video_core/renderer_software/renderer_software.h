// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/renderer_base.h"
#include "video_core/renderer_software/sw_rasterizer.h"

namespace Core {
class System;
}

namespace VideoCore {

class RendererSoftware : public VideoCore::RendererBase {
public:
    explicit RendererSoftware(Core::System& system, Frontend::EmuWindow& window);
    ~RendererSoftware() override;

    [[nodiscard]] VideoCore::RasterizerInterface* Rasterizer() const override {
        return rasterizer.get();
    }

    void SwapBuffers() override;
    void TryPresent(int timeout_ms, bool is_secondary) override {}
    void Sync() override {}

private:
    std::unique_ptr<RasterizerSoftware> rasterizer;
};

} // namespace VideoCore
