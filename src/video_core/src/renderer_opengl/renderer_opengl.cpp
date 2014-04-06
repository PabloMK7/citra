/**
 * Copyright (C) 2014 Citra Emulator
 *
 * @file    renderer_opengl.cpp
 * @author  bunnei
 * @date    2014-04-05
 * @brief   Renderer for OpenGL 3.x
 *
 * @section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * Official project repository can be found at:
 * http://code.google.com/p/gekko-gc-emu/
 */

#include "video_core.h"
#include "renderer_opengl/renderer_opengl.h"

/// RendererOpenGL constructor
RendererOpenGL::RendererOpenGL() {
    memset(fbo_, 0, sizeof(fbo_));  
    memset(fbo_rbo_, 0, sizeof(fbo_rbo_));  
    memset(fbo_depth_buffers_, 0, sizeof(fbo_depth_buffers_));

    resolution_width_ = max(VideoCore::kScreenTopWidth, VideoCore::kScreenBottomWidth);
    resolution_height_ = VideoCore::kScreenTopHeight + VideoCore::kScreenBottomHeight;
}

/// RendererOpenGL destructor
RendererOpenGL::~RendererOpenGL() {
}


/// Swap buffers (render frame)
void RendererOpenGL::SwapBuffers() {

    glClearColor(1.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ResetRenderState();

    // EFB->XFB copy
    // TODO(bunnei): This is a hack and does not belong here. The copy should be triggered by some 
    // register write We're also treating both framebuffers as a single one in OpenGL.
    Rect framebuffer_size(0, 0, VideoCore::kScreenTopWidth, 
        VideoCore::kScreenTopHeight + VideoCore::kScreenBottomHeight);
    CopyToXFB(framebuffer_size, framebuffer_size);

    // XFB->Window copy
    RenderFramebuffer();

    // Swap buffers
    render_window_->PollEvents();
    render_window_->SwapBuffers();

    // Switch back to EFB and clear
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_[kFramebuffer_EFB]);

    RestoreRenderState();
}

/** 
 * Blits the EFB to the external framebuffer (XFB)
 * @param src_rect Source rectangle in EFB to copy
 * @param dst_rect Destination rectangle in EFB to copy to
 * @param dest_height Destination height in pixels
 */
void RendererOpenGL::CopyToXFB(const Rect& src_rect, const Rect& dst_rect) {
    ResetRenderState();

    // Render target is destination framebuffer
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_[kFramebuffer_VirtualXFB]);
    glViewport(0, 0, resolution_width_, resolution_height_);

    // Render source is our EFB
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_[kFramebuffer_EFB]);
    glReadBuffer(GL_COLOR_ATTACHMENT0);

    // Blit
    glBlitFramebuffer(src_rect.x0_, src_rect.y0_, src_rect.x1_, src_rect.y1_, 
                      dst_rect.x0_, dst_rect.y1_, dst_rect.x1_, dst_rect.y0_,
                      GL_COLOR_BUFFER_BIT, GL_LINEAR);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    RestoreRenderState();
}

/**
 * Clear the screen
 * @param rect Screen rectangle to clear
 * @param enable_color Enable color clearing
 * @param enable_alpha Enable alpha clearing
 * @param enable_z Enable depth clearing
 * @param color Clear color
 * @param z Clear depth
 */
void RendererOpenGL::Clear(const Rect& rect, bool enable_color, bool enable_alpha, bool enable_z, 
    u32 color, u32 z) {
    GLboolean const color_mask = enable_color ? GL_TRUE : GL_FALSE;
    GLboolean const alpha_mask = enable_alpha ? GL_TRUE : GL_FALSE;

    ResetRenderState();

    // Clear color
    glColorMask(color_mask,  color_mask,  color_mask,  alpha_mask);
    glClearColor(float((color >> 16) & 0xFF) / 255.0f, float((color >> 8) & 0xFF) / 255.0f,
        float((color >> 0) & 0xFF) / 255.0f, float((color >> 24) & 0xFF) / 255.0f);

    // Clear depth
    glDepthMask(enable_z ? GL_TRUE : GL_FALSE);
    glClearDepth(float(z & 0xFFFFFF) / float(0xFFFFFF));

    // Specify the rectangle of the EFB to clear
    glEnable(GL_SCISSOR_TEST);
    glScissor(rect.x0_, rect.y1_, rect.width(), rect.height());

    // Clear it!
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    RestoreRenderState();
}

/// Sets the renderer viewport location, width, and height
void RendererOpenGL::SetViewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}

/// Sets the renderer depthrange, znear and zfar
void RendererOpenGL::SetDepthRange(double znear, double zfar) {
    glDepthRange(znear, zfar);
}

/* Sets the scissor box
 * @param rect Renderer rectangle to set scissor box to
 */
void RendererOpenGL::SetScissorBox(const Rect& rect) {
    glScissor(rect.x0_, rect.y1_, rect.width(), rect.height());
}

/**
 * Sets the line and point size
 * @param line_width Line width to use
 * @param point_size Point size to use
 */
void RendererOpenGL::SetLinePointSize(f32 line_width, f32 point_size) {
    glLineWidth((GLfloat)line_width);
    glPointSize((GLfloat)point_size);
}

/**
 * Set a specific render mode
 * @param flag Render flags mode to enable
 */
void RendererOpenGL::SetMode(kRenderMode flags) {
    if(flags & kRenderMode_ZComp) {
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    }
    if(flags & kRenderMode_Multipass) {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);          
        glDepthFunc(GL_EQUAL);
    }
    if (flags & kRenderMode_UseDstAlpha) {
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
        glDisable(GL_BLEND);
    }
    last_mode_ |= flags;
}

/// Reset the full renderer API to the NULL state
void RendererOpenGL::ResetRenderState() {
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

/// Restore the full renderer API state - As the game set it
void RendererOpenGL::RestoreRenderState() {
    
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_[kFramebuffer_EFB]);
    
    //gp::XF_UpdateViewport();
    SetViewport(0, 0, resolution_width_, resolution_height_);
    SetDepthRange(0.0f, 1.0f);
    
    //SetGenerationMode();
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    
    //glEnable(GL_SCISSOR_TEST);
    //gp::BP_SetScissorBox();
    glDisable(GL_SCISSOR_TEST);

    //SetColorMask(gp::g_bp_regs.cmode0);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    
    //SetDepthMode();
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    //SetBlendMode(gp::g_bp_regs.cmode0, gp::g_bp_regs.cmode1, true);
    //if (common::g_config->current_renderer_config().enable_wireframe) {
    //    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    //} else {
    //    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    //}
}

/// Initialize the FBO
void RendererOpenGL::InitFramebuffer() {
    // TODO(en): This should probably be implemented with the top screen and bottom screen as 
    // separate framebuffers

    // Init the FBOs
    // -------------

    glGenFramebuffers(kMaxFramebuffers, fbo_); // Generate primary framebuffer
    glGenRenderbuffers(kMaxFramebuffers, fbo_rbo_); // Generate primary RBOs
    glGenRenderbuffers(kMaxFramebuffers, fbo_depth_buffers_); // Generate primary depth buffer

    for (int i = 0; i < kMaxFramebuffers; i++) {
        // Generate color buffer storage
        glBindRenderbuffer(GL_RENDERBUFFER, fbo_rbo_[i]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, resolution_width_, resolution_height_);

        // Generate depth buffer storage
        glBindRenderbuffer(GL_RENDERBUFFER, fbo_depth_buffers_[i]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, resolution_width_, 
            resolution_height_);

        // Attach the buffers
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_[i]);
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            GL_RENDERBUFFER, fbo_depth_buffers_[i]);
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_RENDERBUFFER, fbo_rbo_[i]);

        // Check for completeness
        if (GL_FRAMEBUFFER_COMPLETE == glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)) {
            NOTICE_LOG(RENDER, "framebuffer(%d) initialized ok", i);
        } else {
            ERROR_LOG(RENDER, "couldn't create OpenGL frame buffer");
            exit(1);
        } 
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // Unbind our frame buffer(s)
}

/// Blit the FBO to the OpenGL default framebuffer
void RendererOpenGL::RenderFramebuffer() {

    // Render target is default framebuffer
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glViewport(0, 0, resolution_width_, resolution_height_);

    // Render source is our XFB
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_[kFramebuffer_VirtualXFB]);
    glReadBuffer(GL_COLOR_ATTACHMENT0);

    // Blit
    glBlitFramebuffer(0, 0, resolution_width_, resolution_height_, 0, 0, 
        render_window_->client_area_width(), render_window_->client_area_height(), 
        GL_COLOR_BUFFER_BIT, GL_LINEAR);

    // Update the FPS count
    UpdateFramerate();

    // Rebind EFB
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_[kFramebuffer_EFB]);

    current_frame_++;
}

/// Updates the framerate
void RendererOpenGL::UpdateFramerate() {
}

/** 
 * Set the emulator window to use for renderer
 * @param window EmuWindow handle to emulator window to use for rendering
 */
void RendererOpenGL::SetWindow(EmuWindow* window) {
    render_window_ = window;
}

/// Initialize the renderer
void RendererOpenGL::Init() {
    render_window_->MakeCurrent();
    glShadeModel(GL_SMOOTH);


    glStencilFunc(GL_ALWAYS, 0, 0);
    glBlendFunc(GL_ONE, GL_ONE);

    glViewport(0, 0, resolution_width_, resolution_height_);

    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDepthFunc(GL_LEQUAL);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    glDisable(GL_STENCIL_TEST);
    glEnable(GL_SCISSOR_TEST);

    glScissor(0, 0, resolution_width_, resolution_height_);
    glClearDepth(1.0f);

    GLenum err = glewInit();
    if (GLEW_OK != err) {
        ERROR_LOG(RENDER, " Failed to initialize GLEW! Error message: \"%s\". Exiting...", 
            glewGetErrorString(err));
        exit(-1);
    }

    // Initialize everything else
    // --------------------------

    InitFramebuffer();

    NOTICE_LOG(RENDER, "GL_VERSION: %s\n", glGetString(GL_VERSION));
}

/// Shutdown the renderer
void RendererOpenGL::ShutDown() {
}
