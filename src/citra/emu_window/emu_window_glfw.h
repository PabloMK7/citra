// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <GLFW/glfw3.h>

#include "common/emu_window.h"

class EmuWindow_GLFW : public EmuWindow {
public:
    EmuWindow_GLFW();
    ~EmuWindow_GLFW();

    /// Swap buffers to display the next frame
    void SwapBuffers();

	/// Polls window events
	void PollEvents();

    /// Makes the graphics context current for the caller thread
    void MakeCurrent();
    
    /// Releases (dunno if this is the "right" word) the GLFW context from the caller thread
    void DoneCurrent();

    static void OnKeyEvent(GLFWwindow* win, int key, int scancode, int action, int mods);

    /// Whether the window is still open, and a close request hasn't yet been sent
    const bool IsOpen();

    void ReloadSetKeymaps() override;

private:
    GLFWwindow* m_render_window; ///< Internal GLFW render window

    /// Device id of keyboard for use with KeyMap
    int keyboard_id;
};
