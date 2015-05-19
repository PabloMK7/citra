// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "generated/gl_3_2_core.h"

#include "common/math_util.h"

#include "core/hw/gpu.h"

#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"

class EmuWindow;

class RendererOpenGL : public RendererBase {
public:

    RendererOpenGL();
    ~RendererOpenGL() override;

    /// Swap buffers (render frame)
    void SwapBuffers() override;

    /**
     * Set the emulator window to use for renderer
     * @param window EmuWindow handle to emulator window to use for rendering
     */
    void SetWindow(EmuWindow* window) override;

    /// Initialize the renderer
    void Init() override;

    /// Shutdown the renderer
    void ShutDown() override;

private:
    /// Structure used for storing information about the textures for each 3DS screen
    struct TextureInfo {
        GLuint handle;
        GLsizei width;
        GLsizei height;
        GPU::Regs::PixelFormat format;
        GLenum gl_format;
        GLenum gl_type;
    };

    void InitOpenGLObjects();
    void ConfigureFramebufferTexture(TextureInfo& texture,
                                     const GPU::Regs::FramebufferConfig& framebuffer);
    void DrawScreens();
    void DrawSingleScreenRotated(const TextureInfo& texture, float x, float y, float w, float h);
    void UpdateFramerate();

    // Loads framebuffer from emulated memory into the active OpenGL texture.
    void LoadFBToActiveGLTexture(const GPU::Regs::FramebufferConfig& framebuffer,
                                 const TextureInfo& texture);
    // Fills active OpenGL texture with the given RGB color.
    void LoadColorToActiveGLTexture(u8 color_r, u8 color_g, u8 color_b,
                                    const TextureInfo& texture);

    /// Computes the viewport rectangle
    MathUtil::Rectangle<unsigned> GetViewportExtent();

    EmuWindow*  render_window;                    ///< Handle to render window
    u32         last_mode;                        ///< Last render mode

    int resolution_width;                         ///< Current resolution width
    int resolution_height;                        ///< Current resolution height

    OpenGLState state;

    // OpenGL object IDs
    GLuint vertex_array_handle;
    GLuint vertex_buffer_handle;
    GLuint program_id;
    std::array<TextureInfo, 2> textures;          ///< Textures for top and bottom screens respectively
    // Shader uniform location indices
    GLuint uniform_modelview_matrix;
    GLuint uniform_color_texture;
    // Shader attribute input indices
    GLuint attrib_position;
    GLuint attrib_tex_coord;
};
