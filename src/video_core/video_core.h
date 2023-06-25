// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <functional>
#include <memory>

namespace Frontend {
class EmuWindow;
}

namespace Core {
class System;
}

namespace Memory {
class MemorySystem;
}

namespace VideoCore {

class RendererBase;

extern std::unique_ptr<RendererBase> g_renderer; ///< Renderer plugin

// TODO: Wrap these in a user settings struct along with any other graphics settings (often set from
// qt ui)
extern std::atomic<bool> g_shader_jit_enabled;
extern std::atomic<bool> g_hw_shader_enabled;
extern std::atomic<bool> g_hw_shader_accurate_mul;

extern Memory::MemorySystem* g_memory;

/// Initialize the video core
void Init(Frontend::EmuWindow& emu_window, Frontend::EmuWindow* secondary_window,
          Core::System& system);

/// Shutdown the video core
void Shutdown();

template <class Archive>
void serialize(Archive& ar, const unsigned int file_version);

} // namespace VideoCore
