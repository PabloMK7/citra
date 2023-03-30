// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/alignment.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/math_util.h"
#include "common/microprofile.h"
#include "video_core/pica_state.h"
#include "video_core/regs_framebuffer.h"
#include "video_core/regs_rasterizer.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_shader_gen.h"
#include "video_core/renderer_opengl/gl_vars.h"
#include "video_core/renderer_opengl/pica_to_gl.h"
#include "video_core/renderer_opengl/renderer_opengl.h"
#include "video_core/video_core.h"

namespace OpenGL {

namespace {

MICROPROFILE_DEFINE(OpenGL_VAO, "OpenGL", "Vertex Array Setup", MP_RGB(255, 128, 0));
MICROPROFILE_DEFINE(OpenGL_VS, "OpenGL", "Vertex Shader Setup", MP_RGB(192, 128, 128));
MICROPROFILE_DEFINE(OpenGL_GS, "OpenGL", "Geometry Shader Setup", MP_RGB(128, 192, 128));
MICROPROFILE_DEFINE(OpenGL_Drawing, "OpenGL", "Drawing", MP_RGB(128, 128, 192));
MICROPROFILE_DEFINE(OpenGL_Blits, "OpenGL", "Blits", MP_RGB(100, 100, 255));
MICROPROFILE_DEFINE(OpenGL_CacheManagement, "OpenGL", "Cache Mgmt", MP_RGB(100, 255, 100));

constexpr std::size_t VERTEX_BUFFER_SIZE = 16 * 1024 * 1024;
constexpr std::size_t INDEX_BUFFER_SIZE = 2 * 1024 * 1024;
constexpr std::size_t UNIFORM_BUFFER_SIZE = 2 * 1024 * 1024;
constexpr std::size_t TEXTURE_BUFFER_SIZE = 2 * 1024 * 1024;

GLenum MakePrimitiveMode(Pica::PipelineRegs::TriangleTopology topology) {
    switch (topology) {
    case Pica::PipelineRegs::TriangleTopology::Shader:
    case Pica::PipelineRegs::TriangleTopology::List:
        return GL_TRIANGLES;
    case Pica::PipelineRegs::TriangleTopology::Fan:
        return GL_TRIANGLE_FAN;
    case Pica::PipelineRegs::TriangleTopology::Strip:
        return GL_TRIANGLE_STRIP;
    default:
        UNREACHABLE();
    }
    return GL_TRIANGLES;
}

GLenum MakeAttributeType(Pica::PipelineRegs::VertexAttributeFormat format) {
    switch (format) {
    case Pica::PipelineRegs::VertexAttributeFormat::BYTE:
        return GL_BYTE;
    case Pica::PipelineRegs::VertexAttributeFormat::UBYTE:
        return GL_UNSIGNED_BYTE;
    case Pica::PipelineRegs::VertexAttributeFormat::SHORT:
        return GL_SHORT;
    case Pica::PipelineRegs::VertexAttributeFormat::FLOAT:
        return GL_FLOAT;
    }
    return GL_UNSIGNED_BYTE;
}

} // Anonymous namespace

RasterizerOpenGL::RasterizerOpenGL(Memory::MemorySystem& memory, VideoCore::RendererBase& renderer,
                                   Driver& driver_)
    : VideoCore::RasterizerAccelerated{memory}, driver{driver_}, res_cache{renderer},
      vertex_buffer{driver, GL_ARRAY_BUFFER, VERTEX_BUFFER_SIZE},
      uniform_buffer{driver, GL_UNIFORM_BUFFER, UNIFORM_BUFFER_SIZE},
      index_buffer{driver, GL_ELEMENT_ARRAY_BUFFER, INDEX_BUFFER_SIZE},
      texture_buffer{driver, GL_TEXTURE_BUFFER, TEXTURE_BUFFER_SIZE}, texture_lf_buffer{
                                                                          driver, GL_TEXTURE_BUFFER,
                                                                          TEXTURE_BUFFER_SIZE} {

    // Clipping plane 0 is always enabled for PICA fixed clip plane z <= 0
    state.clip_distance[0] = true;

    // Create a 1x1 clear texture to use in the NULL case,
    // instead of OpenGL's default of solid black
    glGenTextures(1, &default_texture);
    glBindTexture(GL_TEXTURE_2D, default_texture);
    // For some reason alpha 0 wraps around to 1.0, so use 1/255 instead
    u8 framebuffer_data[4] = {0, 0, 0, 1};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, framebuffer_data);

    // Create sampler objects
    for (std::size_t i = 0; i < texture_samplers.size(); ++i) {
        texture_samplers[i].Create();
        state.texture_units[i].sampler = texture_samplers[i].sampler.handle;
    }

    // Create cubemap texture and sampler objects
    texture_cube_sampler.Create();
    state.texture_cube_unit.sampler = texture_cube_sampler.sampler.handle;

    // Generate VAO
    sw_vao.Create();
    hw_vao.Create();

    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &uniform_buffer_alignment);
    uniform_size_aligned_vs =
        Common::AlignUp<std::size_t>(sizeof(Pica::Shader::VSUniformData), uniform_buffer_alignment);
    uniform_size_aligned_fs =
        Common::AlignUp<std::size_t>(sizeof(Pica::Shader::UniformData), uniform_buffer_alignment);

    // Set vertex attributes for software shader path
    state.draw.vertex_array = sw_vao.handle;
    state.draw.vertex_buffer = vertex_buffer.GetHandle();
    state.Apply();

    glVertexAttribPointer(ATTRIBUTE_POSITION, 4, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex),
                          (GLvoid*)offsetof(HardwareVertex, position));
    glEnableVertexAttribArray(ATTRIBUTE_POSITION);

    glVertexAttribPointer(ATTRIBUTE_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex),
                          (GLvoid*)offsetof(HardwareVertex, color));
    glEnableVertexAttribArray(ATTRIBUTE_COLOR);

    glVertexAttribPointer(ATTRIBUTE_TEXCOORD0, 2, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex),
                          (GLvoid*)offsetof(HardwareVertex, tex_coord0));
    glVertexAttribPointer(ATTRIBUTE_TEXCOORD1, 2, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex),
                          (GLvoid*)offsetof(HardwareVertex, tex_coord1));
    glVertexAttribPointer(ATTRIBUTE_TEXCOORD2, 2, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex),
                          (GLvoid*)offsetof(HardwareVertex, tex_coord2));
    glEnableVertexAttribArray(ATTRIBUTE_TEXCOORD0);
    glEnableVertexAttribArray(ATTRIBUTE_TEXCOORD1);
    glEnableVertexAttribArray(ATTRIBUTE_TEXCOORD2);

    glVertexAttribPointer(ATTRIBUTE_TEXCOORD0_W, 1, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex),
                          (GLvoid*)offsetof(HardwareVertex, tex_coord0_w));
    glEnableVertexAttribArray(ATTRIBUTE_TEXCOORD0_W);

    glVertexAttribPointer(ATTRIBUTE_NORMQUAT, 4, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex),
                          (GLvoid*)offsetof(HardwareVertex, normquat));
    glEnableVertexAttribArray(ATTRIBUTE_NORMQUAT);

    glVertexAttribPointer(ATTRIBUTE_VIEW, 3, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex),
                          (GLvoid*)offsetof(HardwareVertex, view));
    glEnableVertexAttribArray(ATTRIBUTE_VIEW);

    // Create render framebuffer
    framebuffer.Create();

    // Allocate and bind texture buffer lut textures
    texture_buffer_lut_lf.Create();
    texture_buffer_lut_rg.Create();
    texture_buffer_lut_rgba.Create();
    state.texture_buffer_lut_lf.texture_buffer = texture_buffer_lut_lf.handle;
    state.texture_buffer_lut_rg.texture_buffer = texture_buffer_lut_rg.handle;
    state.texture_buffer_lut_rgba.texture_buffer = texture_buffer_lut_rgba.handle;
    state.Apply();
    glActiveTexture(TextureUnits::TextureBufferLUT_LF.Enum());
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RG32F, texture_lf_buffer.GetHandle());
    glActiveTexture(TextureUnits::TextureBufferLUT_RG.Enum());
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RG32F, texture_buffer.GetHandle());
    glActiveTexture(TextureUnits::TextureBufferLUT_RGBA.Enum());
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, texture_buffer.GetHandle());

    // Bind index buffer for hardware shader path
    state.draw.vertex_array = hw_vao.handle;
    state.Apply();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer.GetHandle());

#ifdef __APPLE__
    if (driver.GetVendor() == Vendor::Intel) {
        shader_program_manager = std::make_unique<ShaderProgramManager>(
            renderer.GetRenderWindow(), driver, VideoCore::g_separable_shader_enabled);
    } else {
        shader_program_manager =
            std::make_unique<ShaderProgramManager>(renderer.GetRenderWindow(), driver, true);
    }
#else
    shader_program_manager =
        std::make_unique<ShaderProgramManager>(renderer.GetRenderWindow(), driver, !GLES);
#endif

    glEnable(GL_BLEND);

    SyncEntireState();
}

RasterizerOpenGL::~RasterizerOpenGL() = default;

void RasterizerOpenGL::LoadDiskResources(const std::atomic_bool& stop_loading,
                                         const VideoCore::DiskResourceLoadCallback& callback) {
    shader_program_manager->LoadDiskCache(stop_loading, callback);
}

void RasterizerOpenGL::SyncFixedState() {
    SyncClipEnabled();
    SyncCullMode();
    SyncBlendEnabled();
    SyncBlendFuncs();
    SyncBlendColor();
    SyncLogicOp();
    SyncStencilTest();
    SyncDepthTest();
    SyncColorWriteMask();
    SyncStencilWriteMask();
    SyncDepthWriteMask();
}

void RasterizerOpenGL::SetupVertexArray(u8* array_ptr, GLintptr buffer_offset,
                                        GLuint vs_input_index_min, GLuint vs_input_index_max) {
    MICROPROFILE_SCOPE(OpenGL_VAO);
    const auto& vertex_attributes = regs.pipeline.vertex_attributes;
    PAddr base_address = vertex_attributes.GetPhysicalBaseAddress();

    state.draw.vertex_array = hw_vao.handle;
    state.draw.vertex_buffer = vertex_buffer.GetHandle();
    state.Apply();

    std::array<bool, 16> enable_attributes{};

    for (const auto& loader : vertex_attributes.attribute_loaders) {
        if (loader.component_count == 0 || loader.byte_count == 0) {
            continue;
        }

        u32 offset = 0;
        for (u32 comp = 0; comp < loader.component_count && comp < 12; ++comp) {
            u32 attribute_index = loader.GetComponent(comp);
            if (attribute_index < 12) {
                if (vertex_attributes.GetNumElements(attribute_index) != 0) {
                    offset = Common::AlignUp(
                        offset, vertex_attributes.GetElementSizeInBytes(attribute_index));

                    u32 input_reg = regs.vs.GetRegisterForAttribute(attribute_index);
                    GLint size = vertex_attributes.GetNumElements(attribute_index);
                    GLenum type = MakeAttributeType(vertex_attributes.GetFormat(attribute_index));
                    GLsizei stride = loader.byte_count;
                    glVertexAttribPointer(input_reg, size, type, GL_FALSE, stride,
                                          reinterpret_cast<GLvoid*>(buffer_offset + offset));
                    enable_attributes[input_reg] = true;

                    offset += vertex_attributes.GetStride(attribute_index);
                }
            } else {
                // Attribute ids 12, 13, 14 and 15 signify 4, 8, 12 and 16-byte paddings,
                // respectively
                offset = Common::AlignUp(offset, 4);
                offset += (attribute_index - 11) * 4;
            }
        }

        PAddr data_addr =
            base_address + loader.data_offset + (vs_input_index_min * loader.byte_count);

        u32 vertex_num = vs_input_index_max - vs_input_index_min + 1;
        u32 data_size = loader.byte_count * vertex_num;

        res_cache.FlushRegion(data_addr, data_size, nullptr);
        std::memcpy(array_ptr, VideoCore::g_memory->GetPhysicalPointer(data_addr), data_size);

        array_ptr += data_size;
        buffer_offset += data_size;
    }

    for (std::size_t i = 0; i < enable_attributes.size(); ++i) {
        if (enable_attributes[i] != hw_vao_enabled_attributes[i]) {
            if (enable_attributes[i]) {
                glEnableVertexAttribArray(static_cast<GLuint>(i));
            } else {
                glDisableVertexAttribArray(static_cast<GLuint>(i));
            }
            hw_vao_enabled_attributes[i] = enable_attributes[i];
        }

        if (vertex_attributes.IsDefaultAttribute(i)) {
            const u32 reg = regs.vs.GetRegisterForAttribute(i);
            if (!enable_attributes[reg]) {
                const auto& attr = Pica::g_state.input_default_attributes.attr[i];
                glVertexAttrib4f(reg, attr.x.ToFloat32(), attr.y.ToFloat32(), attr.z.ToFloat32(),
                                 attr.w.ToFloat32());
            }
        }
    }
}

bool RasterizerOpenGL::SetupVertexShader() {
    MICROPROFILE_SCOPE(OpenGL_VS);
    return shader_program_manager->UseProgrammableVertexShader(Pica::g_state.regs,
                                                               Pica::g_state.vs);
}

bool RasterizerOpenGL::SetupGeometryShader() {
    MICROPROFILE_SCOPE(OpenGL_GS);

    if (regs.pipeline.use_gs != Pica::PipelineRegs::UseGS::No) {
        LOG_ERROR(Render_OpenGL, "Accelerate draw doesn't support geometry shader");
        return false;
    }

    shader_program_manager->UseFixedGeometryShader(regs);
    return true;
}

bool RasterizerOpenGL::AccelerateDrawBatch(bool is_indexed) {
    if (regs.pipeline.use_gs != Pica::PipelineRegs::UseGS::No) {
        if (regs.pipeline.gs_config.mode != Pica::PipelineRegs::GSMode::Point) {
            return false;
        }
        if (regs.pipeline.triangle_topology != Pica::PipelineRegs::TriangleTopology::Shader) {
            return false;
        }
    }

    if (!SetupVertexShader())
        return false;

    if (!SetupGeometryShader())
        return false;

    return Draw(true, is_indexed);
}

bool RasterizerOpenGL::AccelerateDrawBatchInternal(bool is_indexed) {
    const GLenum primitive_mode = MakePrimitiveMode(regs.pipeline.triangle_topology);
    auto [vs_input_index_min, vs_input_index_max, vs_input_size] = AnalyzeVertexArray(is_indexed);

    if (vs_input_size > VERTEX_BUFFER_SIZE) {
        LOG_WARNING(Render_OpenGL, "Too large vertex input size {}", vs_input_size);
        return false;
    }

    state.draw.vertex_buffer = vertex_buffer.GetHandle();
    state.Apply();

    u8* buffer_ptr;
    GLintptr buffer_offset;
    std::tie(buffer_ptr, buffer_offset, std::ignore) = vertex_buffer.Map(vs_input_size, 4);
    SetupVertexArray(buffer_ptr, buffer_offset, vs_input_index_min, vs_input_index_max);
    vertex_buffer.Unmap(vs_input_size);

    shader_program_manager->ApplyTo(state);
    state.Apply();

    if (is_indexed) {
        bool index_u16 = regs.pipeline.index_array.format != 0;
        std::size_t index_buffer_size = regs.pipeline.num_vertices * (index_u16 ? 2 : 1);

        if (index_buffer_size > INDEX_BUFFER_SIZE) {
            LOG_WARNING(Render_OpenGL, "Too large index input size {}", index_buffer_size);
            return false;
        }

        const u8* index_data = VideoCore::g_memory->GetPhysicalPointer(
            regs.pipeline.vertex_attributes.GetPhysicalBaseAddress() +
            regs.pipeline.index_array.offset);
        std::tie(buffer_ptr, buffer_offset, std::ignore) = index_buffer.Map(index_buffer_size, 4);
        std::memcpy(buffer_ptr, index_data, index_buffer_size);
        index_buffer.Unmap(index_buffer_size);

        glDrawRangeElementsBaseVertex(
            primitive_mode, vs_input_index_min, vs_input_index_max, regs.pipeline.num_vertices,
            index_u16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE,
            reinterpret_cast<const void*>(buffer_offset), -static_cast<GLint>(vs_input_index_min));
    } else {
        glDrawArrays(primitive_mode, 0, regs.pipeline.num_vertices);
    }
    return true;
}

void RasterizerOpenGL::DrawTriangles() {
    if (vertex_batch.empty())
        return;
    Draw(false, false);
}

bool RasterizerOpenGL::Draw(bool accelerate, bool is_indexed) {
    MICROPROFILE_SCOPE(OpenGL_Drawing);

    bool shadow_rendering = regs.framebuffer.output_merger.fragment_operation_mode ==
                            Pica::FramebufferRegs::FragmentOperationMode::Shadow;

    const bool has_stencil =
        regs.framebuffer.framebuffer.depth_format == Pica::FramebufferRegs::DepthFormat::D24S8;

    const bool write_color_fb = shadow_rendering || state.color_mask.red_enabled == GL_TRUE ||
                                state.color_mask.green_enabled == GL_TRUE ||
                                state.color_mask.blue_enabled == GL_TRUE ||
                                state.color_mask.alpha_enabled == GL_TRUE;

    const bool write_depth_fb =
        (state.depth.test_enabled && state.depth.write_mask == GL_TRUE) ||
        (has_stencil && state.stencil.test_enabled && state.stencil.write_mask != 0);

    const bool using_color_fb =
        regs.framebuffer.framebuffer.GetColorBufferPhysicalAddress() != 0 && write_color_fb;
    const bool using_depth_fb =
        !shadow_rendering && regs.framebuffer.framebuffer.GetDepthBufferPhysicalAddress() != 0 &&
        (write_depth_fb || regs.framebuffer.output_merger.depth_test_enable != 0 ||
         (has_stencil && state.stencil.test_enabled));

    Common::Rectangle<s32> viewport_rect_unscaled{
        // These registers hold half-width and half-height, so must be multiplied by 2
        regs.rasterizer.viewport_corner.x,  // left
        regs.rasterizer.viewport_corner.y + // top
            static_cast<s32>(Pica::float24::FromRaw(regs.rasterizer.viewport_size_y).ToFloat32() *
                             2),
        regs.rasterizer.viewport_corner.x + // right
            static_cast<s32>(Pica::float24::FromRaw(regs.rasterizer.viewport_size_x).ToFloat32() *
                             2),
        regs.rasterizer.viewport_corner.y // bottom
    };

    Surface color_surface;
    Surface depth_surface;
    Common::Rectangle<u32> surfaces_rect;
    std::tie(color_surface, depth_surface, surfaces_rect) =
        res_cache.GetFramebufferSurfaces(using_color_fb, using_depth_fb, viewport_rect_unscaled);

    const u16 res_scale = color_surface != nullptr
                              ? color_surface->res_scale
                              : (depth_surface == nullptr ? 1u : depth_surface->res_scale);

    Common::Rectangle<u32> draw_rect{
        static_cast<u32>(std::clamp<s32>(static_cast<s32>(surfaces_rect.left) +
                                             viewport_rect_unscaled.left * res_scale,
                                         surfaces_rect.left, surfaces_rect.right)), // Left
        static_cast<u32>(std::clamp<s32>(static_cast<s32>(surfaces_rect.bottom) +
                                             viewport_rect_unscaled.top * res_scale,
                                         surfaces_rect.bottom, surfaces_rect.top)), // Top
        static_cast<u32>(std::clamp<s32>(static_cast<s32>(surfaces_rect.left) +
                                             viewport_rect_unscaled.right * res_scale,
                                         surfaces_rect.left, surfaces_rect.right)), // Right
        static_cast<u32>(std::clamp<s32>(static_cast<s32>(surfaces_rect.bottom) +
                                             viewport_rect_unscaled.bottom * res_scale,
                                         surfaces_rect.bottom, surfaces_rect.top))}; // Bottom

    // Bind the framebuffer surfaces
    state.draw.draw_framebuffer = framebuffer.handle;
    state.Apply();

    if (shadow_rendering) {
        if (color_surface == nullptr) {
            return true;
        }

        glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH,
                                color_surface->width * color_surface->res_scale);
        glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT,
                                color_surface->height * color_surface->res_scale);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0,
                               0);
        state.image_shadow_buffer = color_surface->texture.handle;
    } else {
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               color_surface != nullptr ? color_surface->texture.handle : 0, 0);
        if (depth_surface != nullptr) {
            if (has_stencil) {
                // attach both depth and stencil
                glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                       GL_TEXTURE_2D, depth_surface->texture.handle, 0);
            } else {
                // attach depth
                glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                                       depth_surface->texture.handle, 0);
                // clear stencil attachment
                glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0,
                                       0);
            }
        } else {
            // clear both depth and stencil attachment
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                                   0, 0);
        }
    }

    // Sync the viewport
    state.viewport.x =
        static_cast<GLint>(surfaces_rect.left) + viewport_rect_unscaled.left * res_scale;
    state.viewport.y =
        static_cast<GLint>(surfaces_rect.bottom) + viewport_rect_unscaled.bottom * res_scale;
    state.viewport.width = static_cast<GLsizei>(viewport_rect_unscaled.GetWidth() * res_scale);
    state.viewport.height = static_cast<GLsizei>(viewport_rect_unscaled.GetHeight() * res_scale);

    if (uniform_block_data.data.framebuffer_scale != res_scale) {
        uniform_block_data.data.framebuffer_scale = res_scale;
        uniform_block_data.dirty = true;
    }

    // Scissor checks are window-, not viewport-relative, which means that if the cached texture
    // sub-rect changes, the scissor bounds also need to be updated.
    GLint scissor_x1 =
        static_cast<GLint>(surfaces_rect.left + regs.rasterizer.scissor_test.x1 * res_scale);
    GLint scissor_y1 =
        static_cast<GLint>(surfaces_rect.bottom + regs.rasterizer.scissor_test.y1 * res_scale);
    // x2, y2 have +1 added to cover the entire pixel area, otherwise you might get cracks when
    // scaling or doing multisampling.
    GLint scissor_x2 =
        static_cast<GLint>(surfaces_rect.left + (regs.rasterizer.scissor_test.x2 + 1) * res_scale);
    GLint scissor_y2 = static_cast<GLint>(surfaces_rect.bottom +
                                          (regs.rasterizer.scissor_test.y2 + 1) * res_scale);

    if (uniform_block_data.data.scissor_x1 != scissor_x1 ||
        uniform_block_data.data.scissor_x2 != scissor_x2 ||
        uniform_block_data.data.scissor_y1 != scissor_y1 ||
        uniform_block_data.data.scissor_y2 != scissor_y2) {

        uniform_block_data.data.scissor_x1 = scissor_x1;
        uniform_block_data.data.scissor_x2 = scissor_x2;
        uniform_block_data.data.scissor_y1 = scissor_y1;
        uniform_block_data.data.scissor_y2 = scissor_y2;
        uniform_block_data.dirty = true;
    }

    bool need_duplicate_texture = false;
    auto CheckBarrier = [&need_duplicate_texture, &color_surface](GLuint handle) {
        if (color_surface && color_surface->texture.handle == handle) {
            need_duplicate_texture = true;
        }
    };

    const auto BindCubeFace = [&](GLuint& target, Pica::TexturingRegs::CubeFace face,
                                  Pica::Texture::TextureInfo& info) {
        info.physical_address = regs.texturing.GetCubePhysicalAddress(face);
        Surface surface = res_cache.GetTextureSurface(info);

        if (surface != nullptr) {
            CheckBarrier(target = surface->texture.handle);
        } else {
            target = 0;
        }
    };

    // Sync and bind the texture surfaces
    const auto pica_textures = regs.texturing.GetTextures();
    for (unsigned texture_index = 0; texture_index < pica_textures.size(); ++texture_index) {
        const auto& texture = pica_textures[texture_index];

        if (texture.enabled) {
            if (texture_index == 0) {
                using TextureType = Pica::TexturingRegs::TextureConfig::TextureType;
                switch (texture.config.type.Value()) {
                case TextureType::Shadow2D: {
                    Surface surface = res_cache.GetTextureSurface(texture);
                    if (surface != nullptr) {
                        CheckBarrier(state.image_shadow_texture_px = surface->texture.handle);
                    } else {
                        state.image_shadow_texture_px = 0;
                    }
                    continue;
                }
                case TextureType::ShadowCube: {
                    using CubeFace = Pica::TexturingRegs::CubeFace;
                    auto info = Pica::Texture::TextureInfo::FromPicaRegister(texture.config,
                                                                             texture.format);
                    BindCubeFace(state.image_shadow_texture_px, CubeFace::PositiveX, info);
                    BindCubeFace(state.image_shadow_texture_nx, CubeFace::NegativeX, info);
                    BindCubeFace(state.image_shadow_texture_py, CubeFace::PositiveY, info);
                    BindCubeFace(state.image_shadow_texture_ny, CubeFace::NegativeY, info);
                    BindCubeFace(state.image_shadow_texture_pz, CubeFace::PositiveZ, info);
                    BindCubeFace(state.image_shadow_texture_nz, CubeFace::NegativeZ, info);
                    continue;
                }
                case TextureType::TextureCube:
                    using CubeFace = Pica::TexturingRegs::CubeFace;
                    TextureCubeConfig config;
                    config.px = regs.texturing.GetCubePhysicalAddress(CubeFace::PositiveX);
                    config.nx = regs.texturing.GetCubePhysicalAddress(CubeFace::NegativeX);
                    config.py = regs.texturing.GetCubePhysicalAddress(CubeFace::PositiveY);
                    config.ny = regs.texturing.GetCubePhysicalAddress(CubeFace::NegativeY);
                    config.pz = regs.texturing.GetCubePhysicalAddress(CubeFace::PositiveZ);
                    config.nz = regs.texturing.GetCubePhysicalAddress(CubeFace::NegativeZ);
                    config.width = texture.config.width;
                    config.format = texture.format;
                    state.texture_cube_unit.texture_cube =
                        res_cache.GetTextureCube(config).texture.handle;

                    texture_cube_sampler.SyncWithConfig(texture.config);
                    state.texture_units[texture_index].texture_2d = 0;
                    continue; // Texture unit 0 setup finished. Continue to next unit
                default:
                    break;
                }

                state.texture_cube_unit.texture_cube = 0;
            }

            texture_samplers[texture_index].SyncWithConfig(texture.config);
            Surface surface = res_cache.GetTextureSurface(texture);
            if (surface != nullptr) {
                CheckBarrier(state.texture_units[texture_index].texture_2d =
                                 surface->texture.handle);
            } else {
                // Can occur when texture addr is null or its memory is unmapped/invalid
                // HACK: In this case, the correct behaviour for the PICA is to use the last
                // rendered colour. But because this would be impractical to implement, the
                // next best alternative is to use a clear texture, essentially skipping
                // the geometry in question.
                // For example: a bug in Pokemon X/Y causes NULL-texture squares to be drawn
                // on the male character's face, which in the OpenGL default appear black.
                state.texture_units[texture_index].texture_2d = default_texture;
            }
        } else {
            state.texture_units[texture_index].texture_2d = 0;
        }
    }

    OGLTexture temp_tex;
    if (need_duplicate_texture) {
        const auto& tuple = GetFormatTuple(color_surface->pixel_format);
        const GLsizei levels = color_surface->max_level + 1;

        // The game is trying to use a surface as a texture and framebuffer at the same time
        // which causes unpredictable behavior on the host.
        // Making a copy to sample from eliminates this issue and seems to be fairly cheap.
        temp_tex.Create();
        temp_tex.Allocate(GL_TEXTURE_2D, levels, tuple.internal_format,
                          color_surface->GetScaledWidth(), color_surface->GetScaledHeight());

        temp_tex.CopyFrom(color_surface->texture, GL_TEXTURE_2D, levels,
                          color_surface->GetScaledWidth(), color_surface->GetScaledHeight());

        for (auto& unit : state.texture_units) {
            if (unit.texture_2d == color_surface->texture.handle) {
                unit.texture_2d = temp_tex.handle;
            }
        }
        for (auto shadow_unit : {&state.image_shadow_texture_nx, &state.image_shadow_texture_ny,
                                 &state.image_shadow_texture_nz, &state.image_shadow_texture_px,
                                 &state.image_shadow_texture_py, &state.image_shadow_texture_pz}) {
            if (*shadow_unit == color_surface->texture.handle) {
                *shadow_unit = temp_tex.handle;
            }
        }
    }

    // Sync and bind the shader
    if (shader_dirty) {
        SetShader();
        shader_dirty = false;
    }

    // Sync the LUTs within the texture buffer
    SyncAndUploadLUTs();
    SyncAndUploadLUTsLF();

    // Sync the uniform data
    UploadUniforms(accelerate);

    // Viewport can have negative offsets or larger
    // dimensions than our framebuffer sub-rect.
    // Enable scissor test to prevent drawing
    // outside of the framebuffer region
    state.scissor.enabled = true;
    state.scissor.x = draw_rect.left;
    state.scissor.y = draw_rect.bottom;
    state.scissor.width = draw_rect.GetWidth();
    state.scissor.height = draw_rect.GetHeight();
    state.Apply();

    // Draw the vertex batch
    bool succeeded = true;
    if (accelerate) {
        succeeded = AccelerateDrawBatchInternal(is_indexed);
    } else {
        state.draw.vertex_array = sw_vao.handle;
        state.draw.vertex_buffer = vertex_buffer.GetHandle();
        shader_program_manager->UseTrivialVertexShader();
        shader_program_manager->UseTrivialGeometryShader();
        shader_program_manager->ApplyTo(state);
        state.Apply();

        std::size_t max_vertices = 3 * (VERTEX_BUFFER_SIZE / (3 * sizeof(HardwareVertex)));
        for (std::size_t base_vertex = 0; base_vertex < vertex_batch.size();
             base_vertex += max_vertices) {
            const std::size_t vertices = std::min(max_vertices, vertex_batch.size() - base_vertex);
            const std::size_t vertex_size = vertices * sizeof(HardwareVertex);
            u8* vbo;
            GLintptr offset;
            std::tie(vbo, offset, std::ignore) =
                vertex_buffer.Map(vertex_size, sizeof(HardwareVertex));
            std::memcpy(vbo, vertex_batch.data() + base_vertex, vertex_size);
            vertex_buffer.Unmap(vertex_size);
            glDrawArrays(GL_TRIANGLES, static_cast<GLint>(offset / sizeof(HardwareVertex)),
                         static_cast<GLsizei>(vertices));
        }
    }

    vertex_batch.clear();

    // Reset textures in rasterizer state context because the rasterizer cache might delete them
    for (unsigned texture_index = 0; texture_index < pica_textures.size(); ++texture_index) {
        state.texture_units[texture_index].texture_2d = 0;
    }
    state.texture_cube_unit.texture_cube = 0;
    state.image_shadow_texture_px = 0;
    state.image_shadow_texture_nx = 0;
    state.image_shadow_texture_py = 0;
    state.image_shadow_texture_ny = 0;
    state.image_shadow_texture_pz = 0;
    state.image_shadow_texture_nz = 0;
    state.image_shadow_buffer = 0;
    state.Apply();

    if (shadow_rendering) {
        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
                        GL_TEXTURE_UPDATE_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);
    }

    // Mark framebuffer surfaces as dirty
    Common::Rectangle<u32> draw_rect_unscaled{draw_rect.left / res_scale, draw_rect.top / res_scale,
                                              draw_rect.right / res_scale,
                                              draw_rect.bottom / res_scale};

    if (color_surface != nullptr && write_color_fb) {
        auto interval = color_surface->GetSubRectInterval(draw_rect_unscaled);
        res_cache.InvalidateRegion(boost::icl::first(interval), boost::icl::length(interval),
                                   color_surface);
    }
    if (depth_surface != nullptr && write_depth_fb) {
        auto interval = depth_surface->GetSubRectInterval(draw_rect_unscaled);
        res_cache.InvalidateRegion(boost::icl::first(interval), boost::icl::length(interval),
                                   depth_surface);
    }

    return succeeded;
}

void RasterizerOpenGL::NotifyFixedFunctionPicaRegisterChanged(u32 id) {
    switch (id) {
    // Clipping plane
    case PICA_REG_INDEX(rasterizer.clip_enable):
        SyncClipEnabled();
        break;

    // Culling
    case PICA_REG_INDEX(rasterizer.cull_mode):
        SyncCullMode();
        break;

    // Blending
    case PICA_REG_INDEX(framebuffer.output_merger.alphablend_enable):
        SyncBlendEnabled();
        // Update since logic op emulation depends on alpha blend enable.
        SyncLogicOp();
        SyncColorWriteMask();
        break;
    case PICA_REG_INDEX(framebuffer.output_merger.alpha_blending):
        SyncBlendFuncs();
        break;
    case PICA_REG_INDEX(framebuffer.output_merger.blend_const):
        SyncBlendColor();
        break;

    // Sync GL stencil test + stencil write mask
    // (Pica stencil test function register also contains a stencil write mask)
    case PICA_REG_INDEX(framebuffer.output_merger.stencil_test.raw_func):
        SyncStencilTest();
        SyncStencilWriteMask();
        break;
    case PICA_REG_INDEX(framebuffer.output_merger.stencil_test.raw_op):
    case PICA_REG_INDEX(framebuffer.framebuffer.depth_format):
        SyncStencilTest();
        break;

    // Sync GL depth test + depth and color write mask
    // (Pica depth test function register also contains a depth and color write mask)
    case PICA_REG_INDEX(framebuffer.output_merger.depth_test_enable):
        SyncDepthTest();
        SyncDepthWriteMask();
        SyncColorWriteMask();
        break;

    // Sync GL depth and stencil write mask
    // (This is a dedicated combined depth / stencil write-enable register)
    case PICA_REG_INDEX(framebuffer.framebuffer.allow_depth_stencil_write):
        SyncDepthWriteMask();
        SyncStencilWriteMask();
        break;

    // Sync GL color write mask
    // (This is a dedicated color write-enable register)
    case PICA_REG_INDEX(framebuffer.framebuffer.allow_color_write):
        SyncColorWriteMask();
        break;

    // Logic op
    case PICA_REG_INDEX(framebuffer.output_merger.logic_op):
        SyncLogicOp();
        // Update since color write mask is used to emulate no-op.
        SyncColorWriteMask();
        break;
    }
}

void RasterizerOpenGL::FlushAll() {
    MICROPROFILE_SCOPE(OpenGL_CacheManagement);
    res_cache.FlushAll();
}

void RasterizerOpenGL::FlushRegion(PAddr addr, u32 size) {
    MICROPROFILE_SCOPE(OpenGL_CacheManagement);
    res_cache.FlushRegion(addr, size);
}

void RasterizerOpenGL::InvalidateRegion(PAddr addr, u32 size) {
    MICROPROFILE_SCOPE(OpenGL_CacheManagement);
    res_cache.InvalidateRegion(addr, size, nullptr);
}

void RasterizerOpenGL::FlushAndInvalidateRegion(PAddr addr, u32 size) {
    MICROPROFILE_SCOPE(OpenGL_CacheManagement);
    res_cache.FlushRegion(addr, size);
    res_cache.InvalidateRegion(addr, size, nullptr);
}

void RasterizerOpenGL::ClearAll(bool flush) {
    res_cache.ClearAll(flush);
}

bool RasterizerOpenGL::AccelerateDisplayTransfer(const GPU::Regs::DisplayTransferConfig& config) {
    MICROPROFILE_SCOPE(OpenGL_Blits);

    SurfaceParams src_params;
    src_params.addr = config.GetPhysicalInputAddress();
    src_params.width = config.output_width;
    src_params.stride = config.input_width;
    src_params.height = config.output_height;
    src_params.is_tiled = !config.input_linear;
    src_params.pixel_format = PixelFormatFromGPUPixelFormat(config.input_format);
    src_params.UpdateParams();

    SurfaceParams dst_params;
    dst_params.addr = config.GetPhysicalOutputAddress();
    dst_params.width = config.scaling != config.NoScale ? config.output_width.Value() / 2
                                                        : config.output_width.Value();
    dst_params.height = config.scaling == config.ScaleXY ? config.output_height.Value() / 2
                                                         : config.output_height.Value();
    dst_params.is_tiled = config.input_linear != config.dont_swizzle;
    dst_params.pixel_format = PixelFormatFromGPUPixelFormat(config.output_format);
    dst_params.UpdateParams();

    Common::Rectangle<u32> src_rect;
    Surface src_surface;
    std::tie(src_surface, src_rect) =
        res_cache.GetSurfaceSubRect(src_params, ScaleMatch::Ignore, true);
    if (src_surface == nullptr)
        return false;

    dst_params.res_scale = src_surface->res_scale;

    Common::Rectangle<u32> dst_rect;
    Surface dst_surface;
    std::tie(dst_surface, dst_rect) =
        res_cache.GetSurfaceSubRect(dst_params, ScaleMatch::Upscale, false);
    if (dst_surface == nullptr)
        return false;

    if (src_surface->is_tiled != dst_surface->is_tiled)
        std::swap(src_rect.top, src_rect.bottom);

    if (config.flip_vertically)
        std::swap(src_rect.top, src_rect.bottom);

    if (!res_cache.BlitSurfaces(src_surface, src_rect, dst_surface, dst_rect))
        return false;

    res_cache.InvalidateRegion(dst_params.addr, dst_params.size, dst_surface);
    return true;
}

bool RasterizerOpenGL::AccelerateTextureCopy(const GPU::Regs::DisplayTransferConfig& config) {
    u32 copy_size = Common::AlignDown(config.texture_copy.size, 16);
    if (copy_size == 0) {
        return false;
    }

    u32 input_gap = config.texture_copy.input_gap * 16;
    u32 input_width = config.texture_copy.input_width * 16;
    if (input_width == 0 && input_gap != 0) {
        return false;
    }
    if (input_gap == 0 || input_width >= copy_size) {
        input_width = copy_size;
        input_gap = 0;
    }
    if (copy_size % input_width != 0) {
        return false;
    }

    u32 output_gap = config.texture_copy.output_gap * 16;
    u32 output_width = config.texture_copy.output_width * 16;
    if (output_width == 0 && output_gap != 0) {
        return false;
    }
    if (output_gap == 0 || output_width >= copy_size) {
        output_width = copy_size;
        output_gap = 0;
    }
    if (copy_size % output_width != 0) {
        return false;
    }

    SurfaceParams src_params;
    src_params.addr = config.GetPhysicalInputAddress();
    src_params.stride = input_width + input_gap; // stride in bytes
    src_params.width = input_width;              // width in bytes
    src_params.height = copy_size / input_width;
    src_params.size = ((src_params.height - 1) * src_params.stride) + src_params.width;
    src_params.end = src_params.addr + src_params.size;

    Common::Rectangle<u32> src_rect;
    Surface src_surface;
    std::tie(src_surface, src_rect) = res_cache.GetTexCopySurface(src_params);
    if (src_surface == nullptr) {
        return false;
    }

    if (output_gap != 0 &&
        (output_width != src_surface->BytesInPixels(src_rect.GetWidth() / src_surface->res_scale) *
                             (src_surface->is_tiled ? 8 : 1) ||
         output_gap % src_surface->BytesInPixels(src_surface->is_tiled ? 64 : 1) != 0)) {
        return false;
    }

    SurfaceParams dst_params = *src_surface;
    dst_params.addr = config.GetPhysicalOutputAddress();
    dst_params.width = src_rect.GetWidth() / src_surface->res_scale;
    dst_params.stride = dst_params.width + src_surface->PixelsInBytes(
                                               src_surface->is_tiled ? output_gap / 8 : output_gap);
    dst_params.height = src_rect.GetHeight() / src_surface->res_scale;
    dst_params.res_scale = src_surface->res_scale;
    dst_params.UpdateParams();

    // Since we are going to invalidate the gap if there is one, we will have to load it first
    const bool load_gap = output_gap != 0;
    Common::Rectangle<u32> dst_rect;
    Surface dst_surface;
    std::tie(dst_surface, dst_rect) =
        res_cache.GetSurfaceSubRect(dst_params, ScaleMatch::Upscale, load_gap);
    if (dst_surface == nullptr) {
        return false;
    }

    if (dst_surface->type == SurfaceType::Texture) {
        return false;
    }

    if (!res_cache.BlitSurfaces(src_surface, src_rect, dst_surface, dst_rect)) {
        return false;
    }

    res_cache.InvalidateRegion(dst_params.addr, dst_params.size, dst_surface);
    return true;
}

bool RasterizerOpenGL::AccelerateFill(const GPU::Regs::MemoryFillConfig& config) {
    Surface dst_surface = res_cache.GetFillSurface(config);
    if (dst_surface == nullptr)
        return false;

    res_cache.InvalidateRegion(dst_surface->addr, dst_surface->size, dst_surface);
    return true;
}

bool RasterizerOpenGL::AccelerateDisplay(const GPU::Regs::FramebufferConfig& config,
                                         PAddr framebuffer_addr, u32 pixel_stride,
                                         ScreenInfo& screen_info) {
    if (framebuffer_addr == 0) {
        return false;
    }
    MICROPROFILE_SCOPE(OpenGL_CacheManagement);

    SurfaceParams src_params;
    src_params.addr = framebuffer_addr;
    src_params.width = std::min(config.width.Value(), pixel_stride);
    src_params.height = config.height;
    src_params.stride = pixel_stride;
    src_params.is_tiled = false;
    src_params.pixel_format = PixelFormatFromGPUPixelFormat(config.color_format);
    src_params.UpdateParams();

    Common::Rectangle<u32> src_rect;
    Surface src_surface;
    std::tie(src_surface, src_rect) =
        res_cache.GetSurfaceSubRect(src_params, ScaleMatch::Ignore, true);

    if (src_surface == nullptr) {
        return false;
    }

    u32 scaled_width = src_surface->GetScaledWidth();
    u32 scaled_height = src_surface->GetScaledHeight();

    screen_info.display_texcoords = Common::Rectangle<float>(
        (float)src_rect.bottom / (float)scaled_height, (float)src_rect.left / (float)scaled_width,
        (float)src_rect.top / (float)scaled_height, (float)src_rect.right / (float)scaled_width);

    screen_info.display_texture = src_surface->texture.handle;

    return true;
}

void RasterizerOpenGL::SamplerInfo::Create() {
    sampler.Create();
    mag_filter = min_filter = mip_filter = TextureConfig::Linear;
    wrap_s = wrap_t = TextureConfig::Repeat;
    border_color = 0;
    lod_min = lod_max = 0;
    lod_bias = 0;

    // default is 1000 and -1000
    // Other attributes have correct defaults
    glSamplerParameterf(sampler.handle, GL_TEXTURE_MAX_LOD, static_cast<float>(lod_max));
    glSamplerParameterf(sampler.handle, GL_TEXTURE_MIN_LOD, static_cast<float>(lod_min));
}

void RasterizerOpenGL::SamplerInfo::SyncWithConfig(
    const Pica::TexturingRegs::TextureConfig& config) {

    GLuint s = sampler.handle;

    if (mag_filter != config.mag_filter) {
        mag_filter = config.mag_filter;
        glSamplerParameteri(s, GL_TEXTURE_MAG_FILTER, PicaToGL::TextureMagFilterMode(mag_filter));
    }

    // TODO(wwylele): remove new_supress_mipmap_for_cube logic once mipmap for cube is implemented
    bool new_supress_mipmap_for_cube =
        config.type == Pica::TexturingRegs::TextureConfig::TextureCube;
    if (min_filter != config.min_filter || mip_filter != config.mip_filter ||
        supress_mipmap_for_cube != new_supress_mipmap_for_cube) {
        min_filter = config.min_filter;
        mip_filter = config.mip_filter;
        supress_mipmap_for_cube = new_supress_mipmap_for_cube;
        if (new_supress_mipmap_for_cube) {
            // HACK: use mag filter converter for min filter because they are the same anyway
            glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER,
                                PicaToGL::TextureMagFilterMode(min_filter));
        } else {
            glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER,
                                PicaToGL::TextureMinFilterMode(min_filter, mip_filter));
        }
    }

    if (wrap_s != config.wrap_s) {
        wrap_s = config.wrap_s;
        glSamplerParameteri(s, GL_TEXTURE_WRAP_S, PicaToGL::WrapMode(wrap_s));
    }
    if (wrap_t != config.wrap_t) {
        wrap_t = config.wrap_t;
        glSamplerParameteri(s, GL_TEXTURE_WRAP_T, PicaToGL::WrapMode(wrap_t));
    }

    if (wrap_s == TextureConfig::ClampToBorder || wrap_t == TextureConfig::ClampToBorder) {
        if (border_color != config.border_color.raw) {
            border_color = config.border_color.raw;
            auto gl_color = PicaToGL::ColorRGBA8(border_color);
            glSamplerParameterfv(s, GL_TEXTURE_BORDER_COLOR, gl_color.AsArray());
        }
    }

    if (lod_min != config.lod.min_level) {
        lod_min = config.lod.min_level;
        glSamplerParameterf(s, GL_TEXTURE_MIN_LOD, static_cast<float>(lod_min));
    }

    if (lod_max != config.lod.max_level) {
        lod_max = config.lod.max_level;
        glSamplerParameterf(s, GL_TEXTURE_MAX_LOD, static_cast<float>(lod_max));
    }

    if (!GLES && lod_bias != config.lod.bias) {
        lod_bias = config.lod.bias;
        glSamplerParameterf(s, GL_TEXTURE_LOD_BIAS, lod_bias / 256.0f);
    }
}

void RasterizerOpenGL::SetShader() {
    shader_program_manager->UseFragmentShader(Pica::g_state.regs);
}

void RasterizerOpenGL::SyncClipEnabled() {
    state.clip_distance[1] = Pica::g_state.regs.rasterizer.clip_enable != 0;
}

void RasterizerOpenGL::SyncCullMode() {
    switch (regs.rasterizer.cull_mode) {
    case Pica::RasterizerRegs::CullMode::KeepAll:
        state.cull.enabled = false;
        break;

    case Pica::RasterizerRegs::CullMode::KeepClockWise:
        state.cull.enabled = true;
        state.cull.front_face = GL_CW;
        break;

    case Pica::RasterizerRegs::CullMode::KeepCounterClockWise:
        state.cull.enabled = true;
        state.cull.front_face = GL_CCW;
        break;

    default:
        LOG_CRITICAL(Render_OpenGL, "Unknown cull mode {}",
                     static_cast<u32>(regs.rasterizer.cull_mode.Value()));
        UNIMPLEMENTED();
        break;
    }
}

void RasterizerOpenGL::SyncBlendEnabled() {
    state.blend.enabled = (Pica::g_state.regs.framebuffer.output_merger.alphablend_enable == 1);
}

void RasterizerOpenGL::SyncBlendFuncs() {
    state.blend.rgb_equation =
        PicaToGL::BlendEquation(regs.framebuffer.output_merger.alpha_blending.blend_equation_rgb);
    state.blend.a_equation =
        PicaToGL::BlendEquation(regs.framebuffer.output_merger.alpha_blending.blend_equation_a);
    state.blend.src_rgb_func =
        PicaToGL::BlendFunc(regs.framebuffer.output_merger.alpha_blending.factor_source_rgb);
    state.blend.dst_rgb_func =
        PicaToGL::BlendFunc(regs.framebuffer.output_merger.alpha_blending.factor_dest_rgb);
    state.blend.src_a_func =
        PicaToGL::BlendFunc(regs.framebuffer.output_merger.alpha_blending.factor_source_a);
    state.blend.dst_a_func =
        PicaToGL::BlendFunc(regs.framebuffer.output_merger.alpha_blending.factor_dest_a);
}

void RasterizerOpenGL::SyncBlendColor() {
    auto blend_color =
        PicaToGL::ColorRGBA8(Pica::g_state.regs.framebuffer.output_merger.blend_const.raw);
    state.blend.color.red = blend_color[0];
    state.blend.color.green = blend_color[1];
    state.blend.color.blue = blend_color[2];
    state.blend.color.alpha = blend_color[3];
}

void RasterizerOpenGL::SyncLogicOp() {
    state.logic_op = PicaToGL::LogicOp(regs.framebuffer.output_merger.logic_op);

    if (GLES) {
        if (!regs.framebuffer.output_merger.alphablend_enable) {
            if (regs.framebuffer.output_merger.logic_op == Pica::FramebufferRegs::LogicOp::NoOp) {
                // Color output is disabled by logic operation. We use color write mask to skip
                // color but allow depth write.
                state.color_mask = {};
            }
        }
    }
}

void RasterizerOpenGL::SyncColorWriteMask() {
    if (GLES) {
        if (!regs.framebuffer.output_merger.alphablend_enable) {
            if (regs.framebuffer.output_merger.logic_op == Pica::FramebufferRegs::LogicOp::NoOp) {
                // Color output is disabled by logic operation. We use color write mask to skip
                // color but allow depth write. Return early to avoid overwriting this.
                return;
            }
        }
    }

    auto IsColorWriteEnabled = [&](u32 value) {
        return (regs.framebuffer.framebuffer.allow_color_write != 0 && value != 0) ? GL_TRUE
                                                                                   : GL_FALSE;
    };

    state.color_mask.red_enabled = IsColorWriteEnabled(regs.framebuffer.output_merger.red_enable);
    state.color_mask.green_enabled =
        IsColorWriteEnabled(regs.framebuffer.output_merger.green_enable);
    state.color_mask.blue_enabled = IsColorWriteEnabled(regs.framebuffer.output_merger.blue_enable);
    state.color_mask.alpha_enabled =
        IsColorWriteEnabled(regs.framebuffer.output_merger.alpha_enable);
}

void RasterizerOpenGL::SyncStencilWriteMask() {
    state.stencil.write_mask =
        (regs.framebuffer.framebuffer.allow_depth_stencil_write != 0)
            ? static_cast<GLuint>(regs.framebuffer.output_merger.stencil_test.write_mask)
            : 0;
}

void RasterizerOpenGL::SyncDepthWriteMask() {
    state.depth.write_mask = (regs.framebuffer.framebuffer.allow_depth_stencil_write != 0 &&
                              regs.framebuffer.output_merger.depth_write_enable)
                                 ? GL_TRUE
                                 : GL_FALSE;
}

void RasterizerOpenGL::SyncStencilTest() {
    state.stencil.test_enabled =
        regs.framebuffer.output_merger.stencil_test.enable &&
        regs.framebuffer.framebuffer.depth_format == Pica::FramebufferRegs::DepthFormat::D24S8;
    state.stencil.test_func =
        PicaToGL::CompareFunc(regs.framebuffer.output_merger.stencil_test.func);
    state.stencil.test_ref = regs.framebuffer.output_merger.stencil_test.reference_value;
    state.stencil.test_mask = regs.framebuffer.output_merger.stencil_test.input_mask;
    state.stencil.action_stencil_fail =
        PicaToGL::StencilOp(regs.framebuffer.output_merger.stencil_test.action_stencil_fail);
    state.stencil.action_depth_fail =
        PicaToGL::StencilOp(regs.framebuffer.output_merger.stencil_test.action_depth_fail);
    state.stencil.action_depth_pass =
        PicaToGL::StencilOp(regs.framebuffer.output_merger.stencil_test.action_depth_pass);
}

void RasterizerOpenGL::SyncDepthTest() {
    state.depth.test_enabled = regs.framebuffer.output_merger.depth_test_enable == 1 ||
                               regs.framebuffer.output_merger.depth_write_enable == 1;
    state.depth.test_func =
        regs.framebuffer.output_merger.depth_test_enable == 1
            ? PicaToGL::CompareFunc(regs.framebuffer.output_merger.depth_test_func)
            : GL_ALWAYS;
}

void RasterizerOpenGL::SyncAndUploadLUTsLF() {
    constexpr std::size_t max_size =
        sizeof(Common::Vec2f) * 256 * Pica::LightingRegs::NumLightingSampler +
        sizeof(Common::Vec2f) * 128; // fog

    if (!uniform_block_data.lighting_lut_dirty_any && !uniform_block_data.fog_lut_dirty) {
        return;
    }

    u8* buffer;
    GLintptr offset;
    bool invalidate;
    std::size_t bytes_used = 0;
    glBindBuffer(GL_TEXTURE_BUFFER, texture_lf_buffer.GetHandle());
    std::tie(buffer, offset, invalidate) = texture_lf_buffer.Map(max_size, sizeof(Common::Vec4f));

    // Sync the lighting luts
    if (uniform_block_data.lighting_lut_dirty_any || invalidate) {
        for (unsigned index = 0; index < uniform_block_data.lighting_lut_dirty.size(); index++) {
            if (uniform_block_data.lighting_lut_dirty[index] || invalidate) {
                std::array<Common::Vec2f, 256> new_data;
                const auto& source_lut = Pica::g_state.lighting.luts[index];
                std::transform(source_lut.begin(), source_lut.end(), new_data.begin(),
                               [](const auto& entry) {
                                   return Common::Vec2f{entry.ToFloat(), entry.DiffToFloat()};
                               });

                if (new_data != lighting_lut_data[index] || invalidate) {
                    lighting_lut_data[index] = new_data;
                    std::memcpy(buffer + bytes_used, new_data.data(),
                                new_data.size() * sizeof(Common::Vec2f));
                    uniform_block_data.data.lighting_lut_offset[index / 4][index % 4] =
                        static_cast<GLint>((offset + bytes_used) / sizeof(Common::Vec2f));
                    uniform_block_data.dirty = true;
                    bytes_used += new_data.size() * sizeof(Common::Vec2f);
                }
                uniform_block_data.lighting_lut_dirty[index] = false;
            }
        }
        uniform_block_data.lighting_lut_dirty_any = false;
    }

    // Sync the fog lut
    if (uniform_block_data.fog_lut_dirty || invalidate) {
        std::array<Common::Vec2f, 128> new_data;

        std::transform(Pica::g_state.fog.lut.begin(), Pica::g_state.fog.lut.end(), new_data.begin(),
                       [](const auto& entry) {
                           return Common::Vec2f{entry.ToFloat(), entry.DiffToFloat()};
                       });

        if (new_data != fog_lut_data || invalidate) {
            fog_lut_data = new_data;
            std::memcpy(buffer + bytes_used, new_data.data(),
                        new_data.size() * sizeof(Common::Vec2f));
            uniform_block_data.data.fog_lut_offset =
                static_cast<int>((offset + bytes_used) / sizeof(Common::Vec2f));
            uniform_block_data.dirty = true;
            bytes_used += new_data.size() * sizeof(Common::Vec2f);
        }
        uniform_block_data.fog_lut_dirty = false;
    }

    texture_lf_buffer.Unmap(bytes_used);
}

void RasterizerOpenGL::SyncAndUploadLUTs() {
    constexpr std::size_t max_size =
        sizeof(Common::Vec2f) * 128 * 3 + // proctex: noise + color + alpha
        sizeof(Common::Vec4f) * 256 +     // proctex
        sizeof(Common::Vec4f) * 256;      // proctex diff

    if (!uniform_block_data.proctex_noise_lut_dirty &&
        !uniform_block_data.proctex_color_map_dirty &&
        !uniform_block_data.proctex_alpha_map_dirty && !uniform_block_data.proctex_lut_dirty &&
        !uniform_block_data.proctex_diff_lut_dirty) {
        return;
    }

    u8* buffer;
    GLintptr offset;
    bool invalidate;
    std::size_t bytes_used = 0;
    glBindBuffer(GL_TEXTURE_BUFFER, texture_buffer.GetHandle());
    std::tie(buffer, offset, invalidate) = texture_buffer.Map(max_size, sizeof(Common::Vec4f));

    // helper function for SyncProcTexNoiseLUT/ColorMap/AlphaMap
    auto SyncProcTexValueLUT = [this, buffer, offset, invalidate, &bytes_used](
                                   const std::array<Pica::State::ProcTex::ValueEntry, 128>& lut,
                                   std::array<Common::Vec2f, 128>& lut_data, GLint& lut_offset) {
        std::array<Common::Vec2f, 128> new_data;
        std::transform(lut.begin(), lut.end(), new_data.begin(), [](const auto& entry) {
            return Common::Vec2f{entry.ToFloat(), entry.DiffToFloat()};
        });

        if (new_data != lut_data || invalidate) {
            lut_data = new_data;
            std::memcpy(buffer + bytes_used, new_data.data(),
                        new_data.size() * sizeof(Common::Vec2f));
            lut_offset = static_cast<GLint>((offset + bytes_used) / sizeof(Common::Vec2f));
            uniform_block_data.dirty = true;
            bytes_used += new_data.size() * sizeof(Common::Vec2f);
        }
    };

    // Sync the proctex noise lut
    if (uniform_block_data.proctex_noise_lut_dirty || invalidate) {
        SyncProcTexValueLUT(Pica::g_state.proctex.noise_table, proctex_noise_lut_data,
                            uniform_block_data.data.proctex_noise_lut_offset);
        uniform_block_data.proctex_noise_lut_dirty = false;
    }

    // Sync the proctex color map
    if (uniform_block_data.proctex_color_map_dirty || invalidate) {
        SyncProcTexValueLUT(Pica::g_state.proctex.color_map_table, proctex_color_map_data,
                            uniform_block_data.data.proctex_color_map_offset);
        uniform_block_data.proctex_color_map_dirty = false;
    }

    // Sync the proctex alpha map
    if (uniform_block_data.proctex_alpha_map_dirty || invalidate) {
        SyncProcTexValueLUT(Pica::g_state.proctex.alpha_map_table, proctex_alpha_map_data,
                            uniform_block_data.data.proctex_alpha_map_offset);
        uniform_block_data.proctex_alpha_map_dirty = false;
    }

    // Sync the proctex lut
    if (uniform_block_data.proctex_lut_dirty || invalidate) {
        std::array<Common::Vec4f, 256> new_data;

        std::transform(Pica::g_state.proctex.color_table.begin(),
                       Pica::g_state.proctex.color_table.end(), new_data.begin(),
                       [](const auto& entry) {
                           auto rgba = entry.ToVector() / 255.0f;
                           return Common::Vec4f{rgba.r(), rgba.g(), rgba.b(), rgba.a()};
                       });

        if (new_data != proctex_lut_data || invalidate) {
            proctex_lut_data = new_data;
            std::memcpy(buffer + bytes_used, new_data.data(),
                        new_data.size() * sizeof(Common::Vec4f));
            uniform_block_data.data.proctex_lut_offset =
                static_cast<GLint>((offset + bytes_used) / sizeof(Common::Vec4f));
            uniform_block_data.dirty = true;
            bytes_used += new_data.size() * sizeof(Common::Vec4f);
        }
        uniform_block_data.proctex_lut_dirty = false;
    }

    // Sync the proctex difference lut
    if (uniform_block_data.proctex_diff_lut_dirty || invalidate) {
        std::array<Common::Vec4f, 256> new_data;

        std::transform(Pica::g_state.proctex.color_diff_table.begin(),
                       Pica::g_state.proctex.color_diff_table.end(), new_data.begin(),
                       [](const auto& entry) {
                           auto rgba = entry.ToVector() / 255.0f;
                           return Common::Vec4f{rgba.r(), rgba.g(), rgba.b(), rgba.a()};
                       });

        if (new_data != proctex_diff_lut_data || invalidate) {
            proctex_diff_lut_data = new_data;
            std::memcpy(buffer + bytes_used, new_data.data(),
                        new_data.size() * sizeof(Common::Vec4f));
            uniform_block_data.data.proctex_diff_lut_offset =
                static_cast<GLint>((offset + bytes_used) / sizeof(Common::Vec4f));
            uniform_block_data.dirty = true;
            bytes_used += new_data.size() * sizeof(Common::Vec4f);
        }
        uniform_block_data.proctex_diff_lut_dirty = false;
    }

    texture_buffer.Unmap(bytes_used);
}

void RasterizerOpenGL::UploadUniforms(bool accelerate_draw) {
    // glBindBufferRange below also changes the generic buffer binding point, so we sync the state
    // first
    state.draw.uniform_buffer = uniform_buffer.GetHandle();
    state.Apply();

    bool sync_vs = accelerate_draw;
    bool sync_fs = uniform_block_data.dirty;

    if (!sync_vs && !sync_fs)
        return;

    std::size_t uniform_size = uniform_size_aligned_vs + uniform_size_aligned_fs;
    std::size_t used_bytes = 0;
    u8* uniforms;
    GLintptr offset;
    bool invalidate;
    std::tie(uniforms, offset, invalidate) =
        uniform_buffer.Map(uniform_size, uniform_buffer_alignment);

    if (sync_vs) {
        Pica::Shader::VSUniformData vs_uniforms;
        vs_uniforms.uniforms.SetFromRegs(Pica::g_state.regs.vs, Pica::g_state.vs);
        std::memcpy(uniforms + used_bytes, &vs_uniforms, sizeof(vs_uniforms));
        glBindBufferRange(GL_UNIFORM_BUFFER, static_cast<GLuint>(Pica::Shader::UniformBindings::VS),
                          uniform_buffer.GetHandle(), offset + used_bytes,
                          sizeof(Pica::Shader::VSUniformData));
        used_bytes += uniform_size_aligned_vs;
    }

    if (sync_fs || invalidate) {
        std::memcpy(uniforms + used_bytes, &uniform_block_data.data,
                    sizeof(Pica::Shader::UniformData));
        glBindBufferRange(
            GL_UNIFORM_BUFFER, static_cast<GLuint>(Pica::Shader::UniformBindings::Common),
            uniform_buffer.GetHandle(), offset + used_bytes, sizeof(Pica::Shader::UniformData));
        uniform_block_data.dirty = false;
        used_bytes += uniform_size_aligned_fs;
    }

    uniform_buffer.Unmap(used_bytes);
}

} // namespace OpenGL
