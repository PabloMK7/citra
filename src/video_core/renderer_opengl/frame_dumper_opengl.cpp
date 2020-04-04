// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <glad/glad.h>
#include "core/frontend/emu_window.h"
#include "core/frontend/scope_acquire_context.h"
#include "video_core/renderer_opengl/frame_dumper_opengl.h"
#include "video_core/renderer_opengl/renderer_opengl.h"

namespace OpenGL {

FrameDumperOpenGL::FrameDumperOpenGL(VideoDumper::Backend& video_dumper_,
                                     Frontend::EmuWindow& emu_window)
    : video_dumper(video_dumper_), context(emu_window.CreateSharedContext()) {}

FrameDumperOpenGL::~FrameDumperOpenGL() {
    if (present_thread.joinable())
        present_thread.join();
}

bool FrameDumperOpenGL::IsDumping() const {
    return video_dumper.IsDumping();
}

Layout::FramebufferLayout FrameDumperOpenGL::GetLayout() const {
    return video_dumper.GetLayout();
}

void FrameDumperOpenGL::StartDumping() {
    if (present_thread.joinable())
        present_thread.join();

    present_thread = std::thread(&FrameDumperOpenGL::PresentLoop, this);
}

void FrameDumperOpenGL::StopDumping() {
    stop_requested.store(true, std::memory_order_relaxed);
}

void FrameDumperOpenGL::PresentLoop() {
    Frontend::ScopeAcquireContext scope{*context};
    InitializeOpenGLObjects();

    const auto& layout = GetLayout();
    while (!stop_requested.exchange(false)) {
        auto frame = mailbox->TryGetPresentFrame(200);
        if (!frame) {
            continue;
        }

        if (frame->color_reloaded) {
            LOG_DEBUG(Render_OpenGL, "Reloading present frame");
            mailbox->ReloadPresentFrame(frame, layout.width, layout.height);
        }
        glWaitSync(frame->render_fence, 0, GL_TIMEOUT_IGNORED);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, frame->present.handle);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[current_pbo].handle);
        glReadPixels(0, 0, layout.width, layout.height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, 0);

        // Insert fence for the main thread to block on
        frame->present_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glFlush();

        // Bind the previous PBO and read the pixels
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[next_pbo].handle);
        GLubyte* pixels = static_cast<GLubyte*>(glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY));
        VideoDumper::VideoFrame frame_data{layout.width, layout.height, pixels};
        video_dumper.AddVideoFrame(std::move(frame_data));
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

        current_pbo = (current_pbo + 1) % 2;
        next_pbo = (current_pbo + 1) % 2;
    }

    CleanupOpenGLObjects();
}

void FrameDumperOpenGL::InitializeOpenGLObjects() {
    const auto& layout = GetLayout();
    for (auto& buffer : pbos) {
        buffer.Create();
        glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer.handle);
        glBufferData(GL_PIXEL_PACK_BUFFER, layout.width * layout.height * 4, nullptr,
                     GL_STREAM_READ);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    }
}

void FrameDumperOpenGL::CleanupOpenGLObjects() {
    for (auto& buffer : pbos) {
        buffer.Release();
    }
}

} // namespace OpenGL
