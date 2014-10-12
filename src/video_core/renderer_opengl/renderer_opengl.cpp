// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "core/hw/gpu.h"
#include "core/mem_map.h"
#include "common/emu_window.h"
#include "video_core/video_core.h"
#include "video_core/renderer_opengl/renderer_opengl.h"
#include "video_core/renderer_opengl/gl_shader_util.h"
#include "video_core/renderer_opengl/gl_shaders.h"

#include <algorithm>

/**
 * Vertex structure that the drawn screen rectangles are composed of.
 */
struct ScreenRectVertex {
    ScreenRectVertex(GLfloat x, GLfloat y, GLfloat u, GLfloat v) {
        position[0] = x;
        position[1] = y;
        tex_coord[0] = u;
        tex_coord[1] = v;
    }

    GLfloat position[2];
    GLfloat tex_coord[2];
};

/**
 * Defines a 1:1 pixel ortographic projection matrix with (0,0) on the top-left
 * corner and (width, height) on the lower-bottom.
 *
 * The projection part of the matrix is trivial, hence these operations are represented
 * by a 3x2 matrix.
 */
static std::array<GLfloat, 3*2> MakeOrthographicMatrix(const float width, const float height) {
    std::array<GLfloat, 3*2> matrix;

    matrix[0] = 2.f / width; matrix[2] = 0.f;           matrix[4] = -1.f;
    matrix[1] = 0.f;         matrix[3] = -2.f / height; matrix[5] = 1.f;
    // Last matrix row is implicitly assumed to be [0, 0, 1].

    return matrix;
}

/// RendererOpenGL constructor
RendererOpenGL::RendererOpenGL() {
    resolution_width  = std::max(VideoCore::kScreenTopWidth, VideoCore::kScreenBottomWidth);
    resolution_height = VideoCore::kScreenTopHeight + VideoCore::kScreenBottomHeight;
}

/// RendererOpenGL destructor
RendererOpenGL::~RendererOpenGL() {
}

/// Swap buffers (render frame)
void RendererOpenGL::SwapBuffers() {
    render_window->MakeCurrent();

    for(int i : {0, 1}) {
        const auto& framebuffer = GPU::g_regs.framebuffer_config[i];

        if (textures[i].width != framebuffer.width || textures[i].height != framebuffer.height) {
            // Reallocate texture if the framebuffer size has changed.
            // This is expected to not happen very often and hence should not be a
            // performance problem.
            glBindTexture(GL_TEXTURE_2D, textures[i].handle);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, framebuffer.width, framebuffer.height, 0,
                GL_BGR, GL_UNSIGNED_BYTE, nullptr);
            textures[i].width = framebuffer.width;
            textures[i].height = framebuffer.height;
        }

        LoadFBToActiveGLTexture(GPU::g_regs.framebuffer_config[i], textures[i]);
    }

    DrawScreens();

    // Swap buffers
    render_window->PollEvents();
    render_window->SwapBuffers();
}

/**
 * Loads framebuffer from emulated memory into the active OpenGL texture.
 */
void RendererOpenGL::LoadFBToActiveGLTexture(const GPU::Regs::FramebufferConfig& framebuffer,
                                             const TextureInfo& texture) {
    const VAddr framebuffer_vaddr = Memory::PhysicalToVirtualAddress(
        framebuffer.active_fb == 1 ? framebuffer.address_left2 : framebuffer.address_left1);

    DEBUG_LOG(GPU, "0x%08x bytes from 0x%08x(%dx%d), fmt %x",
        framebuffer.stride * framebuffer.height,
        framebuffer_vaddr, (int)framebuffer.width,
        (int)framebuffer.height, (int)framebuffer.format);

    const u8* framebuffer_data = Memory::GetPointer(framebuffer_vaddr);

    // TODO: Handle other pixel formats
    _dbg_assert_msg_(RENDER, framebuffer.color_format == GPU::Regs::PixelFormat::RGB8,
                     "Unsupported 3DS pixel format.");

    size_t pixel_stride = framebuffer.stride / 3;
    // OpenGL only supports specifying a stride in units of pixels, not bytes, unfortunately
    _dbg_assert_(RENDER, pixel_stride * 3 == framebuffer.stride);
    // Ensure no bad interactions with GL_UNPACK_ALIGNMENT, which by default
    // only allows rows to have a memory alignement of 4.
    _dbg_assert_(RENDER, pixel_stride % 4 == 0);

    glBindTexture(GL_TEXTURE_2D, texture.handle);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)pixel_stride);

    // Update existing texture
    // TODO: Test what happens on hardware when you change the framebuffer dimensions so that they
    //       differ from the LCD resolution.
    // TODO: Applications could theoretically crash Citra here by specifying too large
    //       framebuffer sizes. We should make sure that this cannot happen.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, framebuffer.width, framebuffer.height,
        GL_BGR, GL_UNSIGNED_BYTE, framebuffer_data);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    glBindTexture(GL_TEXTURE_2D, 0);
}

/**
 * Initializes the OpenGL state and creates persistent objects.
 */
void RendererOpenGL::InitOpenGLObjects() {
    glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
    glDisable(GL_DEPTH_TEST);

    // Link shaders and get variable locations
    program_id = ShaderUtil::LoadShaders(GLShaders::g_vertex_shader, GLShaders::g_fragment_shader);
    uniform_modelview_matrix = glGetUniformLocation(program_id, "modelview_matrix");
    uniform_color_texture = glGetUniformLocation(program_id, "color_texture");
    attrib_position = glGetAttribLocation(program_id, "vert_position");
    attrib_tex_coord = glGetAttribLocation(program_id, "vert_tex_coord");

    // Generate VBO handle for drawing
    glGenBuffers(1, &vertex_buffer_handle);

    // Generate VAO
    glGenVertexArrays(1, &vertex_array_handle);
    glBindVertexArray(vertex_array_handle);

    // Attach vertex data to VAO
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_handle);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ScreenRectVertex) * 4, nullptr, GL_STREAM_DRAW);
    glVertexAttribPointer(attrib_position,  2, GL_FLOAT, GL_FALSE, sizeof(ScreenRectVertex), (GLvoid*)offsetof(ScreenRectVertex, position));
    glVertexAttribPointer(attrib_tex_coord, 2, GL_FLOAT, GL_FALSE, sizeof(ScreenRectVertex), (GLvoid*)offsetof(ScreenRectVertex, tex_coord));
    glEnableVertexAttribArray(attrib_position);
    glEnableVertexAttribArray(attrib_tex_coord);

    // Allocate textures for each screen
    for (auto& texture : textures) {
        glGenTextures(1, &texture.handle);

        // Allocation of storage is deferred until the first frame, when we
        // know the framebuffer size.

        glBindTexture(GL_TEXTURE_2D, texture.handle);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

/**
 * Draws a single texture to the emulator window, rotating the texture to correct for the 3DS's LCD rotation.
 */
void RendererOpenGL::DrawSingleScreenRotated(const TextureInfo& texture, float x, float y, float w, float h) {
    std::array<ScreenRectVertex, 4> vertices = {
        ScreenRectVertex(x,   y,   1.f, 0.f),
        ScreenRectVertex(x+w, y,   1.f, 1.f),
        ScreenRectVertex(x,   y+h, 0.f, 0.f),
        ScreenRectVertex(x+w, y+h, 0.f, 1.f),
    };

    glBindTexture(GL_TEXTURE_2D, texture.handle);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_handle);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices.data());
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

/**
 * Draws the emulated screens to the emulator window.
 */
void RendererOpenGL::DrawScreens() {
    glViewport(0, 0, resolution_width, resolution_height);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(program_id);

    // Set projection matrix
    std::array<GLfloat, 3*2> ortho_matrix = MakeOrthographicMatrix((float)resolution_width, (float)resolution_height);
    glUniformMatrix3x2fv(uniform_modelview_matrix, 1, GL_FALSE, ortho_matrix.data());

    // Bind texture in Texture Unit 0
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(uniform_color_texture, 0);

    const float max_width = std::max((float)VideoCore::kScreenTopWidth, (float)VideoCore::kScreenBottomWidth);
    const float top_x = 0.5f * (max_width - VideoCore::kScreenTopWidth);
    const float bottom_x = 0.5f * (max_width - VideoCore::kScreenBottomWidth);

    DrawSingleScreenRotated(textures[0], top_x, 0,
        (float)VideoCore::kScreenTopWidth, (float)VideoCore::kScreenTopHeight);
    DrawSingleScreenRotated(textures[1], bottom_x, (float)VideoCore::kScreenTopHeight,
        (float)VideoCore::kScreenBottomWidth, (float)VideoCore::kScreenBottomHeight);

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

    NOTICE_LOG(RENDER, "GL_VERSION: %s\n", glGetString(GL_VERSION));
    InitOpenGLObjects();
}

/// Shutdown the renderer
void RendererOpenGL::ShutDown() {
}
