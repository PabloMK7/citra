// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "core/frontend/emu_window.h"

struct ANativeWindow;

class SharedContext_Android : public Frontend::GraphicsContext {
public:
    SharedContext_Android(EGLDisplay egl_display, EGLConfig egl_config,
                          EGLContext egl_share_context);

    ~SharedContext_Android() override;

    void MakeCurrent() override;

    void DoneCurrent() override;

private:
    EGLDisplay egl_display{};
    EGLSurface egl_surface{};
    EGLContext egl_context{};
};

class EmuWindow_Android : public Frontend::EmuWindow {
public:
    EmuWindow_Android(ANativeWindow* surface);
    ~EmuWindow_Android();

    void Present();

    /// Called by the onSurfaceChanges() method to change the surface
    void OnSurfaceChanged(ANativeWindow* surface);

    /// Handles touch event that occur.(Touched or released)
    bool OnTouchEvent(int x, int y, bool pressed);

    /// Handles movement of touch pointer
    void OnTouchMoved(int x, int y);

    void PollEvents() override;
    void MakeCurrent() override;
    void DoneCurrent() override;

    void TryPresenting();
    void StopPresenting();

    std::unique_ptr<GraphicsContext> CreateSharedContext() const override;

private:
    void OnFramebufferSizeChanged();
    bool CreateWindowSurface();
    void DestroyWindowSurface();
    void DestroyContext();

    ANativeWindow* render_window{};
    ANativeWindow* host_window{};

    int window_width{};
    int window_height{};

    EGLConfig egl_config;
    EGLSurface egl_surface{};
    EGLContext egl_context{};
    EGLDisplay egl_display{};

    std::unique_ptr<Frontend::GraphicsContext> core_context;

    enum class PresentingState {
        Initial,
        Running,
        Stopped,
    };
    PresentingState presenting_state{};
};
