// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "generated/gl_3_2_core.h"

#include "common/common.h"
#include "common/emu_window.h"

#include "video_core/renderer_base.h"

#include <array>

class RendererOpenGL : virtual public RendererBase {
public:

    RendererOpenGL();
    ~RendererOpenGL();

    /// Swap buffers (render frame)
    void SwapBuffers();

    /**
     * Renders external framebuffer (XFB)
     * @param src_rect Source rectangle in XFB to copy
     * @param dst_rect Destination rectangle in output framebuffer to copy to
     */
    void RenderXFB(const common::Rect& src_rect, const common::Rect& dst_rect);

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

    /// Structure used for storing information for rendering each 3DS screen
    struct ScreenInfo {
        // Properties
        int width;
        int height;
        int stride; ///< Number of bytes between the coordinates (0,0) and (1,0)

        // OpenGL object IDs
        GLuint texture_id;
        GLuint vertex_buffer_id;

        // Temporary
        u8* flipped_xfb_data;
    };

    /**
    * Helper function to flip framebuffer from left-to-right to top-to-bottom
    * @param raw_data Pointer to input raw framebuffer in V/RAM
    * @param screen_info ScreenInfo structure with screen size and output buffer pointer
    * @todo Early on hack... I'd like to find a more efficient way of doing this /bunnei
    */
    void FlipFramebuffer(const u8* raw_data, ScreenInfo& screen_info);

    EmuWindow*  render_window;                    ///< Handle to render window
    u32         last_mode;                        ///< Last render mode

    int resolution_width;                         ///< Current resolution width
    int resolution_height;                        ///< Current resolution height

    // OpenGL global object IDs
    GLuint vertex_array_id;
    GLuint program_id;
    GLuint sampler_id;
    // Shader attribute input indices
    GLuint attrib_position;
    GLuint attrib_texcoord;

    struct : std::array<ScreenInfo, 2> {
        ScreenInfo& Top() { return (*this)[0]; }
        ScreenInfo& Bottom() { return (*this)[1]; }
    } screen_info;

    // "Flipped" framebuffers translate scanlines from native 3DS left-to-right to top-to-bottom
    // as OpenGL expects them in a texture. There probably is a more efficient way of doing this:
    u8 xfb_top_flipped[VideoCore::kScreenTopWidth * VideoCore::kScreenTopHeight * 4];
    u8 xfb_bottom_flipped[VideoCore::kScreenBottomWidth * VideoCore::kScreenBottomHeight * 4];

};
