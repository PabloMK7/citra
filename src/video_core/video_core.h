// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include "core/core.h"
#include "core/frontend/emu_window.h"

namespace Frontend {
class EmuWindow;
}

class RendererBase;

namespace Memory {
class MemorySystem;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Video Core namespace

namespace VideoCore {

extern std::unique_ptr<RendererBase> g_renderer; ///< Renderer plugin

// TODO: Wrap these in a user settings struct along with any other graphics settings (often set from
// qt ui)
extern std::atomic<bool> g_hw_renderer_enabled;
extern std::atomic<bool> g_shader_jit_enabled;
extern std::atomic<bool> g_hw_shader_enabled;
extern std::atomic<bool> g_hw_shader_accurate_mul;
extern std::atomic<bool> g_renderer_bg_color_update_requested;
extern std::atomic<bool> g_renderer_sampler_update_requested;
extern std::atomic<bool> g_renderer_shader_update_requested;
// Screenshot
extern std::atomic<bool> g_renderer_screenshot_requested;
extern void* g_screenshot_bits;
extern std::function<void()> g_screenshot_complete_callback;
extern Layout::FramebufferLayout g_screenshot_framebuffer_layout;

extern Memory::MemorySystem* g_memory;

/// Initialize the video core
Core::System::ResultStatus Init(Frontend::EmuWindow& emu_window, Memory::MemorySystem& memory);

/// Shutdown the video core
void Shutdown();

/// Request a screenshot of the next frame
void RequestScreenshot(void* data, std::function<void()> callback,
                       const Layout::FramebufferLayout& layout);

u16 GetResolutionScaleFactor();

} // namespace VideoCore
