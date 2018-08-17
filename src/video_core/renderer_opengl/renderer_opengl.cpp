// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <glad/glad.h>
#include "common/assert.h"
#include "common/bit_field.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/frontend/emu_window.h"
#include "core/hw/gpu.h"
#include "core/hw/hw.h"
#include "core/hw/lcd.h"
#include "core/memory.h"
#include "core/settings.h"
#include "core/tracer/recorder.h"
#include "video_core/debug_utils/debug_utils.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_opengl/renderer_opengl.h"
#include "video_core/video_core.h"

static const char vertex_shader[] = R"(
#version 150 core

in vec2 vert_position;
in vec2 vert_tex_coord;
out vec2 frag_tex_coord;

// This is a truncated 3x3 matrix for 2D transformations:
// The upper-left 2x2 submatrix performs scaling/rotation/mirroring.
// The third column performs translation.
// The third row could be used for projection, which we don't need in 2D. It hence is assumed to
// implicitly be [0, 0, 1]
uniform mat3x2 modelview_matrix;

void main() {
    // Multiply input position by the rotscale part of the matrix and then manually translate by
    // the last column. This is equivalent to using a full 3x3 matrix and expanding the vector
    // to `vec3(vert_position.xy, 1.0)`
    gl_Position = vec4(mat2(modelview_matrix) * vert_position + modelview_matrix[2], 0.0, 1.0);
    frag_tex_coord = vert_tex_coord;
}
)";

static const char fragment_shader[] = R"(
#version 150 core

in vec2 frag_tex_coord;
out vec4 color;

uniform sampler2D color_texture;

void main() {
    color = texture(color_texture, frag_tex_coord);
}
)";

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
static std::array<GLfloat, 3 * 2> MakeOrthographicMatrix(const float width, const float height) {
    std::array<GLfloat, 3 * 2> matrix; // Laid out in column-major order

    // clang-format off
    matrix[0] = 2.f / width; matrix[2] = 0.f;           matrix[4] = -1.f;
    matrix[1] = 0.f;         matrix[3] = -2.f / height; matrix[5] = 1.f;
    // Last matrix row is implicitly assumed to be [0, 0, 1].
    // clang-format on

    return matrix;
}

RendererOpenGL::RendererOpenGL(EmuWindow& window) : RendererBase{window} {}
RendererOpenGL::~RendererOpenGL() = default;

/// Swap buffers (render frame)
void RendererOpenGL::SwapBuffers() {
    // Maintain the rasterizer's state as a priority
    OpenGLState prev_state = OpenGLState::GetCurState();
    state.Apply();

    for (int i : {0, 1, 2}) {
        int fb_id = i == 2 ? 1 : 0;
        const auto& framebuffer = GPU::g_regs.framebuffer_config[fb_id];

        // Main LCD (0): 0x1ED02204, Sub LCD (1): 0x1ED02A04
        u32 lcd_color_addr =
            (fb_id == 0) ? LCD_REG_INDEX(color_fill_top) : LCD_REG_INDEX(color_fill_bottom);
        lcd_color_addr = HW::VADDR_LCD + 4 * lcd_color_addr;
        LCD::Regs::ColorFill color_fill = {0};
        LCD::Read(color_fill.raw, lcd_color_addr);

        if (color_fill.is_enabled) {
            LoadColorToActiveGLTexture(color_fill.color_r, color_fill.color_g, color_fill.color_b,
                                       screen_infos[i].texture);

            // Resize the texture in case the framebuffer size has changed
            screen_infos[i].texture.width = 1;
            screen_infos[i].texture.height = 1;
        } else {
            if (screen_infos[i].texture.width != (GLsizei)framebuffer.width ||
                screen_infos[i].texture.height != (GLsizei)framebuffer.height ||
                screen_infos[i].texture.format != framebuffer.color_format) {
                // Reallocate texture if the framebuffer size has changed.
                // This is expected to not happen very often and hence should not be a
                // performance problem.
                ConfigureFramebufferTexture(screen_infos[i].texture, framebuffer);
            }
            LoadFBToScreenInfo(framebuffer, screen_infos[i], i == 1);

            // Resize the texture in case the framebuffer size has changed
            screen_infos[i].texture.width = framebuffer.width;
            screen_infos[i].texture.height = framebuffer.height;
        }
    }

    DrawScreens();

    Core::System::GetInstance().perf_stats.EndSystemFrame();

    // Swap buffers
    render_window.PollEvents();
    render_window.SwapBuffers();

    Core::System::GetInstance().frame_limiter.DoFrameLimiting(CoreTiming::GetGlobalTimeUs());
    Core::System::GetInstance().perf_stats.BeginSystemFrame();

    prev_state.Apply();
    RefreshRasterizerSetting();

    if (Pica::g_debug_context && Pica::g_debug_context->recorder) {
        Pica::g_debug_context->recorder->FrameFinished();
    }
}

/**
 * Loads framebuffer from emulated memory into the active OpenGL texture.
 */
void RendererOpenGL::LoadFBToScreenInfo(const GPU::Regs::FramebufferConfig& framebuffer,
                                        ScreenInfo& screen_info, bool right_eye) {

    if (framebuffer.address_right1 == 0 || framebuffer.address_right2 == 0)
        right_eye = false;

    const PAddr framebuffer_addr =
        framebuffer.active_fb == 0
            ? (!right_eye ? framebuffer.address_left1 : framebuffer.address_right1)
            : (!right_eye ? framebuffer.address_left2 : framebuffer.address_right2);

    LOG_TRACE(Render_OpenGL, "0x{:08x} bytes from 0x{:08x}({}x{}), fmt {:x}",
              framebuffer.stride * framebuffer.height, framebuffer_addr, (int)framebuffer.width,
              (int)framebuffer.height, (int)framebuffer.format);

    int bpp = GPU::Regs::BytesPerPixel(framebuffer.color_format);
    size_t pixel_stride = framebuffer.stride / bpp;

    // OpenGL only supports specifying a stride in units of pixels, not bytes, unfortunately
    ASSERT(pixel_stride * bpp == framebuffer.stride);

    // Ensure no bad interactions with GL_UNPACK_ALIGNMENT, which by default
    // only allows rows to have a memory alignement of 4.
    ASSERT(pixel_stride % 4 == 0);

    if (!Rasterizer()->AccelerateDisplay(framebuffer, framebuffer_addr,
                                         static_cast<u32>(pixel_stride), screen_info)) {
        // Reset the screen info's display texture to its own permanent texture
        screen_info.display_texture = screen_info.texture.resource.handle;
        screen_info.display_texcoords = MathUtil::Rectangle<float>(0.f, 0.f, 1.f, 1.f);

        Memory::RasterizerFlushRegion(framebuffer_addr, framebuffer.stride * framebuffer.height);

        const u8* framebuffer_data = Memory::GetPhysicalPointer(framebuffer_addr);

        state.texture_units[0].texture_2d = screen_info.texture.resource.handle;
        state.Apply();

        glActiveTexture(GL_TEXTURE0);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)pixel_stride);

        // Update existing texture
        // TODO: Test what happens on hardware when you change the framebuffer dimensions so that
        //       they differ from the LCD resolution.
        // TODO: Applications could theoretically crash Citra here by specifying too large
        //       framebuffer sizes. We should make sure that this cannot happen.
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, framebuffer.width, framebuffer.height,
                        screen_info.texture.gl_format, screen_info.texture.gl_type,
                        framebuffer_data);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        state.texture_units[0].texture_2d = 0;
        state.Apply();
    }
}

/**
 * Fills active OpenGL texture with the given RGB color. Since the color is solid, the texture can
 * be 1x1 but will stretch across whatever it's rendered on.
 */
void RendererOpenGL::LoadColorToActiveGLTexture(u8 color_r, u8 color_g, u8 color_b,
                                                const TextureInfo& texture) {
    state.texture_units[0].texture_2d = texture.resource.handle;
    state.Apply();

    glActiveTexture(GL_TEXTURE0);
    u8 framebuffer_data[3] = {color_r, color_g, color_b};

    // Update existing texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, framebuffer_data);

    state.texture_units[0].texture_2d = 0;
    state.Apply();
}

/**
 * Initializes the OpenGL state and creates persistent objects.
 */
void RendererOpenGL::InitOpenGLObjects() {
    glClearColor(Settings::values.bg_red, Settings::values.bg_green, Settings::values.bg_blue,
                 0.0f);

    // Link shaders and get variable locations
    shader.Create(vertex_shader, fragment_shader);
    state.draw.shader_program = shader.handle;
    state.Apply();
    uniform_modelview_matrix = glGetUniformLocation(shader.handle, "modelview_matrix");
    uniform_color_texture = glGetUniformLocation(shader.handle, "color_texture");
    attrib_position = glGetAttribLocation(shader.handle, "vert_position");
    attrib_tex_coord = glGetAttribLocation(shader.handle, "vert_tex_coord");

    // Generate VBO handle for drawing
    vertex_buffer.Create();

    // Generate VAO
    vertex_array.Create();

    state.draw.vertex_array = vertex_array.handle;
    state.draw.vertex_buffer = vertex_buffer.handle;
    state.draw.uniform_buffer = 0;
    state.Apply();

    // Attach vertex data to VAO
    glBufferData(GL_ARRAY_BUFFER, sizeof(ScreenRectVertex) * 4, nullptr, GL_STREAM_DRAW);
    glVertexAttribPointer(attrib_position, 2, GL_FLOAT, GL_FALSE, sizeof(ScreenRectVertex),
                          (GLvoid*)offsetof(ScreenRectVertex, position));
    glVertexAttribPointer(attrib_tex_coord, 2, GL_FLOAT, GL_FALSE, sizeof(ScreenRectVertex),
                          (GLvoid*)offsetof(ScreenRectVertex, tex_coord));
    glEnableVertexAttribArray(attrib_position);
    glEnableVertexAttribArray(attrib_tex_coord);

    // Allocate textures for each screen
    for (auto& screen_info : screen_infos) {
        screen_info.texture.resource.Create();

        // Allocation of storage is deferred until the first frame, when we
        // know the framebuffer size.

        state.texture_units[0].texture_2d = screen_info.texture.resource.handle;
        state.Apply();

        glActiveTexture(GL_TEXTURE0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        screen_info.display_texture = screen_info.texture.resource.handle;
    }

    state.texture_units[0].texture_2d = 0;
    state.Apply();
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

    state.texture_units[0].texture_2d = texture.resource.handle;
    state.Apply();

    glActiveTexture(GL_TEXTURE0);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, texture.width, texture.height, 0,
                 texture.gl_format, texture.gl_type, nullptr);

    state.texture_units[0].texture_2d = 0;
    state.Apply();
}

/**
 * Draws a single texture to the emulator window, rotating the texture to correct for the 3DS's LCD
 * rotation.
 */
void RendererOpenGL::DrawSingleScreenRotated(const ScreenInfo& screen_info, float x, float y,
                                             float w, float h) {
    auto& texcoords = screen_info.display_texcoords;

    std::array<ScreenRectVertex, 4> vertices = {{
        ScreenRectVertex(x, y, texcoords.bottom, texcoords.left),
        ScreenRectVertex(x + w, y, texcoords.bottom, texcoords.right),
        ScreenRectVertex(x, y + h, texcoords.top, texcoords.left),
        ScreenRectVertex(x + w, y + h, texcoords.top, texcoords.right),
    }};

    state.texture_units[0].texture_2d = screen_info.display_texture;
    state.Apply();

    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices.data());
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    state.texture_units[0].texture_2d = 0;
    state.Apply();
}

/**
 * Draws the emulated screens to the emulator window.
 */
void RendererOpenGL::DrawScreens() {
    if (VideoCore::g_renderer_bg_color_update_requested.exchange(false)) {
        // Update background color before drawing
        glClearColor(Settings::values.bg_red, Settings::values.bg_green, Settings::values.bg_blue,
                     0.0f);
    }

    auto layout = render_window.GetFramebufferLayout();
    const auto& top_screen = layout.top_screen;
    const auto& bottom_screen = layout.bottom_screen;

    glViewport(0, 0, layout.width, layout.height);
    glClear(GL_COLOR_BUFFER_BIT);

    // Set projection matrix
    std::array<GLfloat, 3 * 2> ortho_matrix =
        MakeOrthographicMatrix((float)layout.width, (float)layout.height);
    glUniformMatrix3x2fv(uniform_modelview_matrix, 1, GL_FALSE, ortho_matrix.data());

    // Bind texture in Texture Unit 0
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(uniform_color_texture, 0);

    if (layout.top_screen_enabled) {
        if (!Settings::values.toggle_3d) {
            DrawSingleScreenRotated(screen_infos[0], (float)top_screen.left, (float)top_screen.top,
                                    (float)top_screen.GetWidth(), (float)top_screen.GetHeight());
        } else {
            DrawSingleScreenRotated(screen_infos[0], (float)top_screen.left / 2,
                                    (float)top_screen.top, (float)top_screen.GetWidth() / 2,
                                    (float)top_screen.GetHeight());
            DrawSingleScreenRotated(screen_infos[1],
                                    ((float)top_screen.left / 2) + ((float)layout.width / 2),
                                    (float)top_screen.top, (float)top_screen.GetWidth() / 2,
                                    (float)top_screen.GetHeight());
        }
    }
    if (layout.bottom_screen_enabled) {
        if (!Settings::values.toggle_3d) {
            DrawSingleScreenRotated(screen_infos[2], (float)bottom_screen.left,
                                    (float)bottom_screen.top, (float)bottom_screen.GetWidth(),
                                    (float)bottom_screen.GetHeight());
        } else {
            DrawSingleScreenRotated(screen_infos[2], (float)bottom_screen.left / 2,
                                    (float)bottom_screen.top, (float)bottom_screen.GetWidth() / 2,
                                    (float)bottom_screen.GetHeight());
            DrawSingleScreenRotated(screen_infos[2],
                                    ((float)bottom_screen.left / 2) + ((float)layout.width / 2),
                                    (float)bottom_screen.top, (float)bottom_screen.GetWidth() / 2,
                                    (float)bottom_screen.GetHeight());
        }
    }

    m_current_frame++;
}

/// Updates the framerate
void RendererOpenGL::UpdateFramerate() {}

static const char* GetSource(GLenum source) {
#define RET(s)                                                                                     \
    case GL_DEBUG_SOURCE_##s:                                                                      \
        return #s
    switch (source) {
        RET(API);
        RET(WINDOW_SYSTEM);
        RET(SHADER_COMPILER);
        RET(THIRD_PARTY);
        RET(APPLICATION);
        RET(OTHER);
    default:
        UNREACHABLE();
    }
#undef RET
}

static const char* GetType(GLenum type) {
#define RET(t)                                                                                     \
    case GL_DEBUG_TYPE_##t:                                                                        \
        return #t
    switch (type) {
        RET(ERROR);
        RET(DEPRECATED_BEHAVIOR);
        RET(UNDEFINED_BEHAVIOR);
        RET(PORTABILITY);
        RET(PERFORMANCE);
        RET(OTHER);
        RET(MARKER);
    default:
        UNREACHABLE();
    }
#undef RET
}

static void APIENTRY DebugHandler(GLenum source, GLenum type, GLuint id, GLenum severity,
                                  GLsizei length, const GLchar* message, const void* user_param) {
    Log::Level level;
    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        level = Log::Level::Critical;
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        level = Log::Level::Warning;
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
    case GL_DEBUG_SEVERITY_LOW:
        level = Log::Level::Debug;
        break;
    }
    LOG_GENERIC(Log::Class::Render_OpenGL, level, "{} {} {}: {}", GetSource(source), GetType(type),
                id, message);
}

/// Initialize the renderer
Core::System::ResultStatus RendererOpenGL::Init() {
    render_window.MakeCurrent();

    if (GLAD_GL_KHR_debug) {
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(DebugHandler, nullptr);
    }

    const char* gl_version{reinterpret_cast<char const*>(glGetString(GL_VERSION))};
    const char* gpu_vendor{reinterpret_cast<char const*>(glGetString(GL_VENDOR))};
    const char* gpu_model{reinterpret_cast<char const*>(glGetString(GL_RENDERER))};

    LOG_INFO(Render_OpenGL, "GL_VERSION: {}", gl_version);
    LOG_INFO(Render_OpenGL, "GL_VENDOR: {}", gpu_vendor);
    LOG_INFO(Render_OpenGL, "GL_RENDERER: {}", gpu_model);

    Core::Telemetry().AddField(Telemetry::FieldType::UserSystem, "GPU_Vendor", gpu_vendor);
    Core::Telemetry().AddField(Telemetry::FieldType::UserSystem, "GPU_Model", gpu_model);
    Core::Telemetry().AddField(Telemetry::FieldType::UserSystem, "GPU_OpenGL_Version", gl_version);

    if (gpu_vendor == "GDI Generic") {
        return Core::System::ResultStatus::ErrorVideoCore_ErrorGenericDrivers;
    }

    if (!GLAD_GL_VERSION_3_3) {
        return Core::System::ResultStatus::ErrorVideoCore_ErrorBelowGL33;
    }

    InitOpenGLObjects();

    RefreshRasterizerSetting();

    return Core::System::ResultStatus::Success;
}

/// Shutdown the renderer
void RendererOpenGL::ShutDown() {}
