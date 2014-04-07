/**
 * Copyright (C) 2014 Citra Emulator
 *
 * @file    renderer_opengl.h
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

#pragma once

#include <GL/glew.h>


#include "common.h"
#include "emu_window.h"

#include "renderer_base.h"


class RendererOpenGL : virtual public RendererBase {
public:

    static const int kMaxFramebuffers = 2;  ///< Maximum number of framebuffers

    RendererOpenGL();
    ~RendererOpenGL();

    /// Swap buffers (render frame)
    void SwapBuffers();

    /** 
     * Renders external framebuffer (XFB)
     * @param src_rect Source rectangle in XFB to copy
     * @param dst_rect Destination rectangle in output framebuffer to copy to
     */
    void RenderXFB(const Rect& src_rect, const Rect& dst_rect);

    /** 
     * Blits the EFB to the external framebuffer (XFB)
     * @param src_rect Source rectangle in EFB to copy
     * @param dst_rect Destination rectangle in EFB to copy to
     */
    void CopyToXFB(const Rect& src_rect, const Rect& dst_rect);

    /**
     * Clear the screen
     * @param rect Screen rectangle to clear
     * @param enable_color Enable color clearing
     * @param enable_alpha Enable alpha clearing
     * @param enable_z Enable depth clearing
     * @param color Clear color
     * @param z Clear depth
     */
    void Clear(const Rect& rect, bool enable_color, bool enable_alpha, bool enable_z, 
        u32 color, u32 z);

    /// Sets the renderer viewport location, width, and height
    void SetViewport(int x, int y, int width, int height);

    /// Sets the renderer depthrange, znear and zfar
    void SetDepthRange(double znear, double zfar);

    /* Sets the scissor box
     * @param rect Renderer rectangle to set scissor box to
     */
    void SetScissorBox(const Rect& rect);

    /**
     * Sets the line and point size
     * @param line_width Line width to use
     * @param point_size Point size to use
     */
    void SetLinePointSize(f32 line_width, f32 point_size);

    /**
     * Set a specific render mode
     * @param flag Render flags mode to enable
     */
    void SetMode(kRenderMode flags);

    /// Reset the full renderer API to the NULL state
    void ResetRenderState();

    /// Restore the full renderer API state - As the game set it
    void RestoreRenderState();

    /** 
     * Set the emulator window to use for renderer
     * @param window EmuWindow handle to emulator window to use for rendering
     */
    void SetWindow(EmuWindow* window);

    /// Initialize the renderer
    void Init();

    /// Shutdown the renderer
    void ShutDown();

    // Framebuffer object(s)
    // ---------------------

    GLuint fbo_[kMaxFramebuffers];  ///< Framebuffer objects

private:

    /// Initialize the FBO
    void InitFramebuffer();

    // Blit the FBO to the OpenGL default framebuffer
    void RenderFramebuffer();

    /// Updates the framerate
    void UpdateFramerate();

    EmuWindow*  render_window_;
    u32         last_mode_;                         ///< Last render mode

    int resolution_width_;
    int resolution_height_;

    // Render buffers
    // --------------

    GLuint fbo_rbo_[kMaxFramebuffers];              ///< Render buffer objects
    GLuint fbo_depth_buffers_[kMaxFramebuffers];    ///< Depth buffers objects

    // External framebuffers
    // ---------------------

    GLuint xfb_texture_top_;                         ///< GL handle to top framebuffer texture
    GLuint xfb_texture_bottom_;                      ///< GL handle to bottom framebuffer texture

    GLuint xfb_top_;
    GLuint xfb_bottom_;

    DISALLOW_COPY_AND_ASSIGN(RendererOpenGL);
};