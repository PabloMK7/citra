// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <utility>
#include "common/emu_window.h"

struct SDL_Window;

class EmuWindow_SDL2 : public EmuWindow {
public:
    EmuWindow_SDL2();
    ~EmuWindow_SDL2();

    /// Swap buffers to display the next frame
    void SwapBuffers() override;

    /// Polls window events
    void PollEvents() override;

    /// Makes the graphics context current for the caller thread
    void MakeCurrent() override;

    /// Releases the GL context from the caller thread
    void DoneCurrent() override;

    /// Whether the window is still open, and a close request hasn't yet been sent
    bool IsOpen() const;

    /// Load keymap from configuration
    void ReloadSetKeymaps() override;

private:
    /// Called by PollEvents when a key is pressed or released.
    void OnKeyEvent(int key, u8 state);

    /// Called by PollEvents when the mouse moves.
    void OnMouseMotion(s32 x, s32 y);

    /// Called by PollEvents when a mouse button is pressed or released
    void OnMouseButton(u32 button, u8 state, s32 x, s32 y);

    /// Called by PollEvents when any event that may cause the window to be resized occurs
    void OnResize();

    /// Called when a configuration change affects the minimal size of the window
    void OnMinimalClientAreaChangeRequest(
        const std::pair<unsigned, unsigned>& minimal_size) override;

    /// Is the window still open?
    bool is_open = true;

    /// Internal SDL2 render window
    SDL_Window* render_window;

    using SDL_GLContext = void*;
    /// The OpenGL context associated with the window
    SDL_GLContext gl_context;

    /// Device id of keyboard for use with KeyMap
    int keyboard_id;
};
