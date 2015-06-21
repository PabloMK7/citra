// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>

class EmuWindow;
class RendererBase;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Video Core namespace

namespace VideoCore {

// 3DS Video Constants
// -------------------

// NOTE: The LCDs actually rotate the image 90 degrees when displaying. Because of that the
// framebuffers in video memory are stored in column-major order and rendered sideways, causing
// the widths and heights of the framebuffers read by the LCD to be switched compared to the
// heights and widths of the screens listed here.
static const int kScreenTopWidth        = 400;  ///< 3DS top screen width
static const int kScreenTopHeight       = 240;  ///< 3DS top screen height
static const int kScreenBottomWidth     = 320;  ///< 3DS bottom screen width
static const int kScreenBottomHeight    = 240;  ///< 3DS bottom screen height

//  Video core renderer
// ---------------------

extern RendererBase*   g_renderer;              ///< Renderer plugin
extern EmuWindow*      g_emu_window;            ///< Emu window

// TODO: Wrap this in a user settings struct along with any other graphics settings (often set from qt ui)
extern std::atomic<bool> g_hw_renderer_enabled;

/// Start the video core
void Start();

/// Initialize the video core
void Init(EmuWindow* emu_window);

/// Shutdown the video core
void Shutdown();

} // namespace
