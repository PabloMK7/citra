// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "generated/gl_3_2_core.h"

#include "common/common.h"
#include "core/hw/gpu.h"
#include "video_core/renderer_base.h"

#include <array>

class EmuWindow;

class RendererOpenGL : public RendererBase {
public:

    RendererOpenGL();
    ~RendererOpenGL() override;

    /// Swap buffers (render frame)
    void SwapBuffers();

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
    /// Structure used for storing information about the textures for each 3DS screen
    struct TextureInfo {
        GLuint handle;
        GLsizei width;
        GLsizei height;
    };

    void InitOpenGLObjects();
    void DrawScreens();
    void DrawSingleScreenRotated(const TextureInfo& texture, float x, float y, float w, float h);
    void UpdateFramerate();

    // Loads framebuffer from emulated memory into the active OpenGL texture.
    static void LoadFBToActiveGLTexture(const GPU::Regs::FramebufferConfig& framebuffer,
                                        const TextureInfo& texture);

    EmuWindow*  render_window;                    ///< Handle to render window
    u32         last_mode;                        ///< Last render mode

    int resolution_width;                         ///< Current resolution width
    int resolution_height;                        ///< Current resolution height

    // OpenGL object IDs
    GLuint vertex_array_handle;
    GLuint vertex_buffer_handle;
    GLuint program_id;
    std::array<TextureInfo, 2> textures;
    // Shader uniform location indices
    GLuint uniform_modelview_matrix;
    GLuint uniform_color_texture;
    // Shader attribute input indices
    GLuint attrib_position;
    GLuint attrib_tex_coord;
};
