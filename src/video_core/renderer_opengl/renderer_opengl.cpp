// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <cstdlib>

#include "common/assert.h"
#include "common/emu_window.h"
#include "common/logging/log.h"
#include "common/profiler_reporting.h"

#include "core/hw/gpu.h"
#include "core/hw/hw.h"
#include "core/hw/lcd.h"
#include "core/memory.h"
#include "core/settings.h"

#include "video_core/video_core.h"
#include "video_core/renderer_opengl/renderer_opengl.h"
#include "video_core/renderer_opengl/gl_shader_util.h"
#include "video_core/renderer_opengl/gl_shaders.h"

#include "video_core/debug_utils/debug_utils.h"

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
    hw_rasterizer.reset(new RasterizerOpenGL());
    resolution_width  = std::max(VideoCore::kScreenTopWidth, VideoCore::kScreenBottomWidth);
    resolution_height = VideoCore::kScreenTopHeight + VideoCore::kScreenBottomHeight;
}

/// RendererOpenGL destructor
RendererOpenGL::~RendererOpenGL() {
}

/// Swap buffers (render frame)
void RendererOpenGL::SwapBuffers() {
    // Maintain the rasterizer's state as a priority
    OpenGLState prev_state = OpenGLState::GetCurState();
    state.Apply();

    for(int i : {0, 1}) {
        const auto& framebuffer = GPU::g_regs.framebuffer_config[i];

        // Main LCD (0): 0x1ED02204, Sub LCD (1): 0x1ED02A04
        u32 lcd_color_addr = (i == 0) ? LCD_REG_INDEX(color_fill_top) : LCD_REG_INDEX(color_fill_bottom);
        lcd_color_addr = HW::VADDR_LCD + 4 * lcd_color_addr;
        LCD::Regs::ColorFill color_fill = {0};
        LCD::Read(color_fill.raw, lcd_color_addr);

        if (color_fill.is_enabled) {
            LoadColorToActiveGLTexture(color_fill.color_r, color_fill.color_g, color_fill.color_b, textures[i]);

            // Resize the texture in case the framebuffer size has changed
            textures[i].width = 1;
            textures[i].height = 1;
        } else {
            if (textures[i].width != (GLsizei)framebuffer.width ||
                textures[i].height != (GLsizei)framebuffer.height ||
                textures[i].format != framebuffer.color_format) {
                // Reallocate texture if the framebuffer size has changed.
                // This is expected to not happen very often and hence should not be a
                // performance problem.
                ConfigureFramebufferTexture(textures[i], framebuffer);
            }
            LoadFBToActiveGLTexture(framebuffer, textures[i]);

            // Resize the texture in case the framebuffer size has changed
            textures[i].width = framebuffer.width;
            textures[i].height = framebuffer.height;
        }
    }

    DrawScreens();

    auto& profiler = Common::Profiling::GetProfilingManager();
    profiler.FinishFrame();
    {
        auto aggregator = Common::Profiling::GetTimingResultsAggregator();
        aggregator->AddFrame(profiler.GetPreviousFrameResults());
    }

    // Swap buffers
    render_window->PollEvents();
    render_window->SwapBuffers();

    prev_state.Apply();

    profiler.BeginFrame();

    bool hw_renderer_enabled = VideoCore::g_hw_renderer_enabled;
    if (Settings::values.use_hw_renderer != hw_renderer_enabled) {
        // TODO: Save new setting value to config file for next startup
        Settings::values.use_hw_renderer = hw_renderer_enabled;

        if (Settings::values.use_hw_renderer) {
            hw_rasterizer->Reset();
        }
    }

    if (Pica::g_debug_context && Pica::g_debug_context->recorder) {
        Pica::g_debug_context->recorder->FrameFinished();
    }
}

/**
 * Loads framebuffer from emulated memory into the active OpenGL texture.
 */
void RendererOpenGL::LoadFBToActiveGLTexture(const GPU::Regs::FramebufferConfig& framebuffer,
                                             const TextureInfo& texture) {

    const PAddr framebuffer_addr = framebuffer.active_fb == 0 ?
            framebuffer.address_left1 : framebuffer.address_left2;

    LOG_TRACE(Render_OpenGL, "0x%08x bytes from 0x%08x(%dx%d), fmt %x",
        framebuffer.stride * framebuffer.height,
        framebuffer_addr, (int)framebuffer.width,
        (int)framebuffer.height, (int)framebuffer.format);

    const u8* framebuffer_data = Memory::GetPhysicalPointer(framebuffer_addr);

    int bpp = GPU::Regs::BytesPerPixel(framebuffer.color_format);
    size_t pixel_stride = framebuffer.stride / bpp;

    // OpenGL only supports specifying a stride in units of pixels, not bytes, unfortunately
    ASSERT(pixel_stride * bpp == framebuffer.stride);

    // Ensure no bad interactions with GL_UNPACK_ALIGNMENT, which by default
    // only allows rows to have a memory alignement of 4.
    ASSERT(pixel_stride % 4 == 0);

    state.texture_units[0].enabled_2d = true;
    state.texture_units[0].texture_2d = texture.handle;
    state.Apply();

    glActiveTexture(GL_TEXTURE0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)pixel_stride);

    // Update existing texture
    // TODO: Test what happens on hardware when you change the framebuffer dimensions so that they
    //       differ from the LCD resolution.
    // TODO: Applications could theoretically crash Citra here by specifying too large
    //       framebuffer sizes. We should make sure that this cannot happen.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, framebuffer.width, framebuffer.height,
                    texture.gl_format, texture.gl_type, framebuffer_data);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    state.texture_units[0].texture_2d = 0;
    state.Apply();
}

/**
 * Fills active OpenGL texture with the given RGB color.
 * Since the color is solid, the texture can be 1x1 but will stretch across whatever it's rendered on.
 * This has the added benefit of being *really fast*.
 */
void RendererOpenGL::LoadColorToActiveGLTexture(u8 color_r, u8 color_g, u8 color_b,
                                                const TextureInfo& texture) {
    state.texture_units[0].enabled_2d = true;
    state.texture_units[0].texture_2d = texture.handle;
    state.Apply();

    glActiveTexture(GL_TEXTURE0);
    u8 framebuffer_data[3] = { color_r, color_g, color_b };

    // Update existing texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, framebuffer_data);
}

/**
 * Initializes the OpenGL state and creates persistent objects.
 */
void RendererOpenGL::InitOpenGLObjects() {
    glClearColor(Settings::values.bg_red, Settings::values.bg_green, Settings::values.bg_blue, 0.0f);

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

    state.draw.vertex_array = vertex_array_handle;
    state.draw.vertex_buffer = vertex_buffer_handle;
    state.Apply();

    // Attach vertex data to VAO
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

        state.texture_units[0].enabled_2d = true;
        state.texture_units[0].texture_2d = texture.handle;
        state.Apply();

        glActiveTexture(GL_TEXTURE0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    state.texture_units[0].texture_2d = 0;
    state.Apply();

    hw_rasterizer->InitObjects();
}

void RendererOpenGL::ConfigureFramebufferTexture(TextureInfo& texture,
                                                 const GPU::Regs::FramebufferConfig& framebuffer) {
    GPU::Regs::PixelFormat format = framebuffer.color_format;
    GLint internal_format;

    texture.format = format;
    texture.width = framebuffer.width;
    texture.height = framebuffer.height;

    switch (format) {
    case GPU::Regs::PixelFormat::RGBA8:
        internal_format = GL_RGBA;
        texture.gl_format = GL_RGBA;
        texture.gl_type = GL_UNSIGNED_INT_8_8_8_8;
        break;

    case GPU::Regs::PixelFormat::RGB8:
        // This pixel format uses BGR since GL_UNSIGNED_BYTE specifies byte-order, unlike every
        // specific OpenGL type used in this function using native-endian (that is, little-endian
        // mostly everywhere) for words or half-words.
        // TODO: check how those behave on big-endian processors.
        internal_format = GL_RGB;
        texture.gl_format = GL_BGR;
        texture.gl_type = GL_UNSIGNED_BYTE;
        break;

    case GPU::Regs::PixelFormat::RGB565:
        internal_format = GL_RGB;
        texture.gl_format = GL_RGB;
        texture.gl_type = GL_UNSIGNED_SHORT_5_6_5;
        break;

    case GPU::Regs::PixelFormat::RGB5A1:
        internal_format = GL_RGBA;
        texture.gl_format = GL_RGBA;
        texture.gl_type = GL_UNSIGNED_SHORT_5_5_5_1;
        break;

    case GPU::Regs::PixelFormat::RGBA4:
        internal_format = GL_RGBA;
        texture.gl_format = GL_RGBA;
        texture.gl_type = GL_UNSIGNED_SHORT_4_4_4_4;
        break;

    default:
        UNIMPLEMENTED();
    }

    state.texture_units[0].enabled_2d = true;
    state.texture_units[0].texture_2d = texture.handle;
    state.Apply();

    glActiveTexture(GL_TEXTURE0);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, texture.width, texture.height, 0,
            texture.gl_format, texture.gl_type, nullptr);
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

    state.texture_units[0].enabled_2d = true;
    state.texture_units[0].texture_2d = texture.handle;
    state.Apply();

    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices.data());
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

/**
 * Draws the emulated screens to the emulator window.
 */
void RendererOpenGL::DrawScreens() {
    auto layout = render_window->GetFramebufferLayout();

    glViewport(0, 0, layout.width, layout.height);
    glClear(GL_COLOR_BUFFER_BIT);

    state.draw.shader_program = program_id;
    state.Apply();

    // Set projection matrix
    std::array<GLfloat, 3 * 2> ortho_matrix = MakeOrthographicMatrix((float)layout.width,
        (float)layout.height);
    glUniformMatrix3x2fv(uniform_modelview_matrix, 1, GL_FALSE, ortho_matrix.data());

    // Bind texture in Texture Unit 0
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(uniform_color_texture, 0);

    DrawSingleScreenRotated(textures[0], (float)layout.top_screen.left, (float)layout.top_screen.top,
        (float)layout.top_screen.GetWidth(), (float)layout.top_screen.GetHeight());
    DrawSingleScreenRotated(textures[1], (float)layout.bottom_screen.left,(float)layout.bottom_screen.top,
        (float)layout.bottom_screen.GetWidth(), (float)layout.bottom_screen.GetHeight());

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
        LOG_CRITICAL(Render_OpenGL, "Failed to initialize GL functions! Exiting...");
        exit(-1);
    }

    LOG_INFO(Render_OpenGL, "GL_VERSION: %s", glGetString(GL_VERSION));
    LOG_INFO(Render_OpenGL, "GL_VENDOR: %s", glGetString(GL_VENDOR));
    LOG_INFO(Render_OpenGL, "GL_RENDERER: %s", glGetString(GL_RENDERER));
    InitOpenGLObjects();
}

/// Shutdown the renderer
void RendererOpenGL::ShutDown() {
}
