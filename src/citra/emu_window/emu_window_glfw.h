// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <utility>

#include "common/emu_window.h"

struct GLFWwindow;

class EmuWindow_GLFW : public EmuWindow {
public:
    EmuWindow_GLFW();
    ~EmuWindow_GLFW();

    /// Swap buffers to display the next frame
    void SwapBuffers() override;

    /// Polls window events
    void PollEvents() override;

    /// Makes the graphics context current for the caller thread
    void MakeCurrent() override;

    /// Releases (dunno if this is the "right" word) the GLFW context from the caller thread
    void DoneCurrent() override;

    static void OnKeyEvent(GLFWwindow* win, int key, int scancode, int action, int mods);

    static void OnMouseButtonEvent(GLFWwindow* window, int button, int action, int mods);

    static void OnCursorPosEvent(GLFWwindow* window, double x, double y);

    /// Whether the window is still open, and a close request hasn't yet been sent
    const bool IsOpen();

    static void OnClientAreaResizeEvent(GLFWwindow* win, int width, int height);

    static void OnFramebufferResizeEvent(GLFWwindow* win, int width, int height);

    void ReloadSetKeymaps() override;

private:
    void OnMinimalClientAreaChangeRequest(const std::pair<unsigned,unsigned>& minimal_size) override;

    static EmuWindow_GLFW* GetEmuWindow(GLFWwindow* win);

    GLFWwindow* m_render_window; ///< Internal GLFW render window

    /// Device id of keyboard for use with KeyMap
    int keyboard_id;
};
