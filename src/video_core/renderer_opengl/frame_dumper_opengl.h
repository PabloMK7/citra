// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include "core/dumping/backend.h"
#include "core/frontend/framebuffer_layout.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace Frontend {
class EmuWindow;
class GraphicsContext;
class TextureMailbox;
} // namespace Frontend

namespace OpenGL {

class RendererOpenGL;

/**
 * This is the 'presentation' part in frame dumping.
 * Processes frames/textures sent to its mailbox, downloads the pixels and sends the data
 * to the video encoding backend.
 */
class FrameDumperOpenGL {
public:
    explicit FrameDumperOpenGL(VideoDumper::Backend& video_dumper, Frontend::EmuWindow& emu_window);
    ~FrameDumperOpenGL();

    bool IsDumping() const;
    Layout::FramebufferLayout GetLayout() const;
    void StartDumping();
    void StopDumping();

    std::unique_ptr<Frontend::TextureMailbox> mailbox;

private:
    void InitializeOpenGLObjects();
    void CleanupOpenGLObjects();
    void PresentLoop();

    VideoDumper::Backend& video_dumper;
    std::unique_ptr<Frontend::GraphicsContext> context;
    std::thread present_thread;
    std::atomic_bool stop_requested{false};

    // PBOs used to dump frames faster
    std::array<OGLBuffer, 2> pbos;
    GLuint current_pbo = 1;
    GLuint next_pbo = 0;
};

} // namespace OpenGL
