// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>

class EmuWindow;
class RendererBase;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Video Core namespace

namespace VideoCore {

extern std::unique_ptr<RendererBase> g_renderer; ///< Renderer plugin
extern EmuWindow* g_emu_window;                  ///< Emu window

// TODO: Wrap these in a user settings struct along with any other graphics settings (often set from
// qt ui)
extern std::atomic<bool> g_hw_renderer_enabled;
extern std::atomic<bool> g_shader_jit_enabled;

/// Start the video core
void Start();

/// Initialize the video core
bool Init(EmuWindow* emu_window);

/// Shutdown the video core
void Shutdown();

} // namespace VideoCore
