// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include "core/core.h"

class EmuWindow;
class RendererBase;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Video Core namespace

namespace VideoCore {

extern std::unique_ptr<RendererBase> g_renderer; ///< Renderer plugin

// TODO: Wrap these in a user settings struct along with any other graphics settings (often set from
// qt ui)
extern std::atomic<bool> g_hw_renderer_enabled;
extern std::atomic<bool> g_shader_jit_enabled;
extern std::atomic<bool> g_hw_shader_enabled;
extern std::atomic<bool> g_hw_shader_accurate_gs;
extern std::atomic<bool> g_hw_shader_accurate_mul;
extern std::atomic<bool> g_renderer_bg_color_update_requested;

/// Initialize the video core
Core::System::ResultStatus Init(EmuWindow& emu_window);

/// Shutdown the video core
void Shutdown();

} // namespace VideoCore
