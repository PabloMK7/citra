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
     * Set the emulator window to use for renderer
     * @param window EmuWindow handle to emulator window to use for rendering
     */
    void SetWindow(EmuWindow* window);

    /// Initialize the renderer
    void Init();

    /// Shutdown the renderer
    void ShutDown();

private:

    /// Initialize the FBO
    void InitFramebuffer();

    // Blit the FBO to the OpenGL default framebuffer
    void RenderFramebuffer();

    /// Updates the framerate
    void UpdateFramerate();

    /**
     * Helper function to flip framebuffer from left-to-right to top-to-bottom
     * @param addr Address of framebuffer in RAM
     * @param out Pointer to output buffer with flipped framebuffer
     * @todo Early on hack... I'd like to find a more efficient way of doing this /bunnei
     */
    void RendererOpenGL::FlipFramebuffer(u32 addr, u8* out);


    EmuWindow*  m_render_window;                    ///< Handle to render window
    u32         m_last_mode;                        ///< Last render mode

    int m_resolution_width;                         ///< Current resolution width
    int m_resolution_height;                        ///< Current resolution height

    // Framebuffers
    // ------------

    GLuint m_fbo[kMaxFramebuffers];                 ///< Framebuffer objects
    GLuint m_fbo_rbo[kMaxFramebuffers];             ///< Render buffer objects
    GLuint m_fbo_depth_buffers[kMaxFramebuffers];   ///< Depth buffers objects

    GLuint m_xfb_texture_top;                       ///< GL handle to top framebuffer texture
    GLuint m_xfb_texture_bottom;                    ///< GL handle to bottom framebuffer texture
           
    GLuint m_xfb_top;                               ///< GL handle to top framebuffer
    GLuint m_xfb_bottom;                            ///< GL handle to bottom framebuffer

    // "Flipped" framebuffers translate scanlines from native 3DS left-to-right to top-to-bottom
    // as OpenGL expects them in a texture. There probably is a more efficient way of doing this:

    u8 m_xfb_top_flipped[VideoCore::kScreenTopWidth * VideoCore::kScreenTopWidth * 4]; 
    u8 m_xfb_bottom_flipped[VideoCore::kScreenTopWidth * VideoCore::kScreenTopWidth * 4];   

    DISALLOW_COPY_AND_ASSIGN(RendererOpenGL);
};