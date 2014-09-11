// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "core/hw/gpu.h"

#include "video_core/video_core.h"
#include "video_core/renderer_opengl/renderer_opengl.h"
#include "video_core/renderer_opengl/gl_shader_util.h"
#include "video_core/renderer_opengl/gl_shaders.h"

#include "core/mem_map.h"

#include <algorithm>

static const GLfloat kViewportAspectRatio =
    (static_cast<float>(VideoCore::kScreenTopHeight) + VideoCore::kScreenBottomHeight) / VideoCore::kScreenTopWidth;

// Fullscreen quad dimensions
static const GLfloat kTopScreenWidthNormalized = 2;
static const GLfloat kTopScreenHeightNormalized = kTopScreenWidthNormalized * (static_cast<float>(VideoCore::kScreenTopHeight) / VideoCore::kScreenTopWidth);
static const GLfloat kBottomScreenWidthNormalized = kTopScreenWidthNormalized * (static_cast<float>(VideoCore::kScreenBottomWidth) / VideoCore::kScreenTopWidth);
static const GLfloat kBottomScreenHeightNormalized = kBottomScreenWidthNormalized * (static_cast<float>(VideoCore::kScreenBottomHeight) / VideoCore::kScreenBottomWidth);

static const GLfloat g_vbuffer_top[] = {
    // x, y, z                                u, v
    -1.0f, 0.0f, 0.0f,                        0.0f, 1.0f,
    1.0f, 0.0f, 0.0f,                         1.0f, 1.0f,
    1.0f, kTopScreenHeightNormalized, 0.0f,   1.0f, 0.0f,
    1.0f, kTopScreenHeightNormalized, 0.0f,   1.0f, 0.0f,
    -1.0f, kTopScreenHeightNormalized, 0.0f,  0.0f, 0.0f,
    -1.0f, 0.0f, 0.0f,                        0.0f, 1.0f
};

static const GLfloat g_vbuffer_bottom[] = {
    // x, y, z                                                                   u, v
    -(kBottomScreenWidthNormalized / 2), -kBottomScreenHeightNormalized, 0.0f,   0.0f, 1.0f,
    (kBottomScreenWidthNormalized / 2), -kBottomScreenHeightNormalized, 0.0f,    1.0f, 1.0f,
    (kBottomScreenWidthNormalized / 2), 0.0f, 0.0f,                              1.0f, 0.0f,
    (kBottomScreenWidthNormalized / 2), 0.0f, 0.0f,                              1.0f, 0.0f,
    -(kBottomScreenWidthNormalized / 2), 0.0f, 0.0f,                             0.0f, 0.0f,
    -(kBottomScreenWidthNormalized / 2), -kBottomScreenHeightNormalized, 0.0f,   0.0f, 1.0f
};

/// RendererOpenGL constructor
RendererOpenGL::RendererOpenGL() {

    resolution_width  = std::max(VideoCore::kScreenTopWidth, VideoCore::kScreenBottomWidth);
    resolution_height = VideoCore::kScreenTopHeight + VideoCore::kScreenBottomHeight;

    // Initialize screen info
    const auto& framebuffer_top = GPU::g_regs.framebuffer_config[0];
    const auto& framebuffer_sub = GPU::g_regs.framebuffer_config[1];

    screen_info.Top().width               = VideoCore::kScreenTopWidth;
    screen_info.Top().height              = VideoCore::kScreenTopHeight;
    screen_info.Top().stride              = framebuffer_top.stride;
    screen_info.Top().flipped_xfb_data    = xfb_top_flipped;

    screen_info.Bottom().width            = VideoCore::kScreenBottomWidth;
    screen_info.Bottom().height           = VideoCore::kScreenBottomHeight;
    screen_info.Bottom().stride           = framebuffer_sub.stride;
    screen_info.Bottom().flipped_xfb_data = xfb_bottom_flipped;
}

/// RendererOpenGL destructor
RendererOpenGL::~RendererOpenGL() {
}

/// Swap buffers (render frame)
void RendererOpenGL::SwapBuffers() {
    render_window->MakeCurrent();

    // EFB->XFB copy
    // TODO(bunnei): This is a hack and does not belong here. The copy should be triggered by some
    // register write.
    //
    // TODO(princesspeachum): (related to above^) this should only be called when there's new data, not every frame.
    // Currently this uploads data that shouldn't have changed.
    Common::Rect framebuffer_size(0, 0, resolution_width, resolution_height);
    RenderXFB(framebuffer_size, framebuffer_size);

    // XFB->Window copy
    RenderFramebuffer();

    // Swap buffers
    render_window->PollEvents();
    render_window->SwapBuffers();
}

/**
 * Helper function to flip framebuffer from left-to-right to top-to-bottom
 * @param raw_data Pointer to input raw framebuffer in V/RAM
 * @param screen_info ScreenInfo structure with screen size and output buffer pointer
 * @todo Early on hack... I'd like to find a more efficient way of doing this /bunnei
 */
void RendererOpenGL::FlipFramebuffer(const u8* raw_data, ScreenInfo& screen_info) {
    for (int x = 0; x < screen_info.width; x++) {
        int in_coord = x * screen_info.stride;
        for (int y = screen_info.height-1; y >= 0; y--) {
            // TODO: Properly support other framebuffer formats
            int out_coord = (x + y * screen_info.width) * 3;
            screen_info.flipped_xfb_data[out_coord] = raw_data[in_coord + 2];       // Red
            screen_info.flipped_xfb_data[out_coord + 1] = raw_data[in_coord + 1];   // Green
            screen_info.flipped_xfb_data[out_coord + 2] = raw_data[in_coord];       // Blue
            in_coord += 3;
        }
    }
}

/**
 * Renders external framebuffer (XFB)
 * @param src_rect Source rectangle in XFB to copy
 * @param dst_rect Destination rectangle in output framebuffer to copy to
 */
void RendererOpenGL::RenderXFB(const Common::Rect& src_rect, const Common::Rect& dst_rect) {
    const auto& framebuffer_top = GPU::g_regs.framebuffer_config[0];
    const auto& framebuffer_sub = GPU::g_regs.framebuffer_config[1];
    const u32 active_fb_top = (framebuffer_top.active_fb == 1)
                            ? Memory::PhysicalToVirtualAddress(framebuffer_top.address_left2)
                            : Memory::PhysicalToVirtualAddress(framebuffer_top.address_left1);
    const u32 active_fb_sub = (framebuffer_sub.active_fb == 1)
                            ? Memory::PhysicalToVirtualAddress(framebuffer_sub.address_left2)
                            : Memory::PhysicalToVirtualAddress(framebuffer_sub.address_left1);

    DEBUG_LOG(GPU, "RenderXFB: 0x%08x bytes from 0x%08x(%dx%d), fmt %x",
              framebuffer_top.stride * framebuffer_top.height,
              active_fb_top, (int)framebuffer_top.width,
              (int)framebuffer_top.height, (int)framebuffer_top.format);

    FlipFramebuffer(Memory::GetPointer(active_fb_top), screen_info.Top());
    FlipFramebuffer(Memory::GetPointer(active_fb_sub), screen_info.Bottom());

    for (int i = 0; i < 2; i++) {
        ScreenInfo* current_screen = &screen_info[i];

        glBindTexture(GL_TEXTURE_2D, current_screen->texture_id);

        // TODO: This should consider the GPU registers for framebuffer width, height and stride.
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, current_screen->width, current_screen->height,
                        GL_RGB, GL_UNSIGNED_BYTE, current_screen->flipped_xfb_data);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    // TODO(princesspeachum):
    // Only the subset src_rect of the GPU buffer
    // should be copied into the texture of the relevant screen.
    //
    // The method's parameters also only include src_rect and dest_rec for one screen,
    // so this may need to be changed (pair for each screen).
}

/// Initialize the FBO
void RendererOpenGL::InitFramebuffer() {
    program_id = ShaderUtil::LoadShaders(GLShaders::g_vertex_shader, GLShaders::g_fragment_shader);
    sampler_id = glGetUniformLocation(program_id, "sampler");
    attrib_position = glGetAttribLocation(program_id, "position");
    attrib_texcoord = glGetAttribLocation(program_id, "texCoord");

    // Generate vertex buffers for both screens
    glGenBuffers(1, &screen_info.Top().vertex_buffer_id);
    glGenBuffers(1, &screen_info.Bottom().vertex_buffer_id);

    // Attach vertex data for top screen
    glBindBuffer(GL_ARRAY_BUFFER, screen_info.Top().vertex_buffer_id);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_vbuffer_top), g_vbuffer_top, GL_STATIC_DRAW);

    // Attach vertex data for bottom screen
    glBindBuffer(GL_ARRAY_BUFFER, screen_info.Bottom().vertex_buffer_id);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_vbuffer_bottom), g_vbuffer_bottom, GL_STATIC_DRAW);

    // Create color buffers for both screens
    glGenTextures(1, &screen_info.Top().texture_id);
    glGenTextures(1, &screen_info.Bottom().texture_id);

    for (int i = 0; i < 2; i++) {

        ScreenInfo* current_screen = &screen_info[i];

        // Allocate texture
        glBindTexture(GL_TEXTURE_2D, current_screen->vertex_buffer_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, current_screen->width, current_screen->height,
                     0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

void RendererOpenGL::RenderFramebuffer() {
    glViewport(0, 0, resolution_width, resolution_height);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(program_id);

    // Bind texture in Texture Unit 0
    glActiveTexture(GL_TEXTURE0);

    glEnableVertexAttribArray(attrib_position);
    glEnableVertexAttribArray(attrib_texcoord);

    for (int i = 0; i < 2; i++) {

        ScreenInfo* current_screen = &screen_info[i];

        glBindTexture(GL_TEXTURE_2D, current_screen->texture_id);

        // Set sampler on Texture Unit 0
        glUniform1i(sampler_id, 0);

        glBindBuffer(GL_ARRAY_BUFFER, current_screen->vertex_buffer_id);

        // Vertex buffer layout
        const GLsizei stride = 5 * sizeof(GLfloat);
        const GLvoid* uv_offset = (const GLvoid*)(3 * sizeof(GLfloat));

        // Configure vertex buffer
        glVertexAttribPointer(attrib_position, 3, GL_FLOAT, GL_FALSE, stride, NULL);
        glVertexAttribPointer(attrib_texcoord, 2, GL_FLOAT, GL_FALSE, stride, uv_offset);

        // Draw screen
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glDisableVertexAttribArray(attrib_position);
    glDisableVertexAttribArray(attrib_texcoord);

    m_current_frame++;
}

/// Updates the framerate
void RendererOpenGL::UpdateFramerate() {
}

/**
 * Set the emulator window to use for renderer
 * @param window EmuWindow handle to emulator window to use for rendering
 */
void RendererOpenGL::SetWindow(EmuWindow* window) {
    render_window = window;
}

/// Initialize the renderer
void RendererOpenGL::Init() {
    render_window->MakeCurrent();

    int err = ogl_LoadFunctions();
    if (ogl_LOAD_SUCCEEDED != err) {
        ERROR_LOG(RENDER, "Failed to initialize GL functions! Exiting...");
        exit(-1);
    }

    // Generate VAO
    glGenVertexArrays(1, &vertex_array_id);
    glBindVertexArray(vertex_array_id);

    glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
    glDisable(GL_DEPTH_TEST);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    // Initialize everything else
    // --------------------------
    InitFramebuffer();

    NOTICE_LOG(RENDER, "GL_VERSION: %s\n", glGetString(GL_VERSION));
}

/// Shutdown the renderer
void RendererOpenGL::ShutDown() {
}
