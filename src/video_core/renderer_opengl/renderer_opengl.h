// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <GL/glew.h>

#include "common/common.h"
#include "common/emu_window.h"

#include "video_core/renderer_base.h"


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
     * @param in Pointer to input raw framebuffer in V/RAM
     * @param out Pointer to output buffer with flipped framebuffer
     * @todo Early on hack... I'd like to find a more efficient way of doing this /bunnei
     */
    void RendererOpenGL::FlipFramebuffer(const u8* in, u8* out);


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