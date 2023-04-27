// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/scope_exit.h"
#include "video_core/custom_textures/material.h"
#include "video_core/regs.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/gl_driver.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/gl_texture_runtime.h"

namespace OpenGL {

namespace {

using VideoCore::MapType;
using VideoCore::PixelFormat;
using VideoCore::SurfaceType;
using VideoCore::TextureType;

constexpr FormatTuple DEFAULT_TUPLE = {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};

static constexpr std::array<FormatTuple, 4> DEPTH_TUPLES = {{
    {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT}, // D16
    {},
    {GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT},   // D24
    {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8}, // D24S8
}};

static constexpr std::array<FormatTuple, 5> COLOR_TUPLES = {{
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8},     // RGBA8
    {GL_RGB8, GL_BGR, GL_UNSIGNED_BYTE},              // RGB8
    {GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1}, // RGB5A1
    {GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5},     // RGB565
    {GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4},   // RGBA4
}};

static constexpr std::array<FormatTuple, 5> COLOR_TUPLES_OES = {{
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE},            // RGBA8
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE},            // RGB8
    {GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1}, // RGB5A1
    {GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5},     // RGB565
    {GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4},   // RGBA4
}};

static constexpr std::array<FormatTuple, 8> CUSTOM_TUPLES = {{
    DEFAULT_TUPLE,
    {GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, GL_UNSIGNED_BYTE},
    {GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_UNSIGNED_BYTE},
    {GL_COMPRESSED_RG_RGTC2, GL_COMPRESSED_RG_RGTC2, GL_UNSIGNED_BYTE},
    {GL_COMPRESSED_RGBA_BPTC_UNORM_ARB, GL_COMPRESSED_RGBA_BPTC_UNORM_ARB, GL_UNSIGNED_BYTE},
    {GL_COMPRESSED_RGBA_ASTC_4x4, GL_COMPRESSED_RGBA_ASTC_4x4, GL_UNSIGNED_BYTE},
    {GL_COMPRESSED_RGBA_ASTC_6x6, GL_COMPRESSED_RGBA_ASTC_6x6, GL_UNSIGNED_BYTE},
    {GL_COMPRESSED_RGBA_ASTC_8x6, GL_COMPRESSED_RGBA_ASTC_8x6, GL_UNSIGNED_BYTE},
}};

struct FramebufferInfo {
    GLuint color;
    GLuint depth;
    u32 color_level;
    u32 depth_level;
};

[[nodiscard]] GLbitfield MakeBufferMask(SurfaceType type) {
    switch (type) {
    case SurfaceType::Color:
    case SurfaceType::Texture:
    case SurfaceType::Fill:
        return GL_COLOR_BUFFER_BIT;
    case SurfaceType::Depth:
        return GL_DEPTH_BUFFER_BIT;
    case SurfaceType::DepthStencil:
        return GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    default:
        UNREACHABLE_MSG("Invalid surface type!");
    }
    return GL_COLOR_BUFFER_BIT;
}

[[nodiscard]] u32 FboIndex(VideoCore::SurfaceType type) {
    switch (type) {
    case VideoCore::SurfaceType::Color:
    case VideoCore::SurfaceType::Texture:
    case VideoCore::SurfaceType::Fill:
        return 0;
    case VideoCore::SurfaceType::Depth:
        return 1;
    case VideoCore::SurfaceType::DepthStencil:
        return 2;
    default:
        UNREACHABLE_MSG("Invalid surface type!");
    }
    return 0;
}

[[nodiscard]] OGLTexture MakeHandle(GLenum target, u32 width, u32 height, u32 levels,
                                    const FormatTuple& tuple, std::string_view debug_name = "") {
    OGLTexture texture{};
    texture.Create();

    glBindTexture(target, texture.handle);
    glTexStorage2D(target, levels, tuple.internal_format, width, height);

    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (!debug_name.empty()) {
        glObjectLabel(GL_TEXTURE, texture.handle, -1, debug_name.data());
    }

    return texture;
}

} // Anonymous namespace

TextureRuntime::TextureRuntime(const Driver& driver_, VideoCore::RendererBase& renderer)
    : driver{driver_}, blit_helper{*this} {
    for (std::size_t i = 0; i < draw_fbos.size(); ++i) {
        draw_fbos[i].Create();
        read_fbos[i].Create();
    }

    auto add_reinterpreter = [this](PixelFormat dest,
                                    std::unique_ptr<FormatReinterpreterBase>&& obj) {
        const u32 dst_index = static_cast<u32>(dest);
        return reinterpreters[dst_index].push_back(std::move(obj));
    };

    add_reinterpreter(PixelFormat::RGBA8, std::make_unique<ShaderD24S8toRGBA8>());
    add_reinterpreter(PixelFormat::RGB5A1, std::make_unique<RGBA4toRGB5A1>());
}

TextureRuntime::~TextureRuntime() = default;

void TextureRuntime::Reset() {
    alloc_cache.clear();
    framebuffer_cache.clear();
}

bool TextureRuntime::NeedsConversion(VideoCore::PixelFormat pixel_format) const {
    const bool should_convert = pixel_format == PixelFormat::RGBA8 || // Needs byteswap
                                pixel_format == PixelFormat::RGB8;    // Is converted to RGBA8
    return driver.IsOpenGLES() && should_convert;
}

VideoCore::StagingData TextureRuntime::FindStaging(u32 size, bool upload) {
    if (size > staging_buffer.size()) {
        staging_buffer.resize(size);
    }
    return VideoCore::StagingData{
        .size = size,
        .mapped = std::span{staging_buffer.data(), size},
        .buffer_offset = 0,
    };
}

const FormatTuple& TextureRuntime::GetFormatTuple(PixelFormat pixel_format) const {
    const auto type = GetFormatType(pixel_format);
    const std::size_t format_index = static_cast<std::size_t>(pixel_format);

    if (type == SurfaceType::Color) {
        ASSERT(format_index < COLOR_TUPLES.size());
        return (driver.IsOpenGLES() ? COLOR_TUPLES_OES : COLOR_TUPLES)[format_index];
    } else if (type == SurfaceType::Depth || type == SurfaceType::DepthStencil) {
        const std::size_t tuple_idx = format_index - 14;
        ASSERT(tuple_idx < DEPTH_TUPLES.size());
        return DEPTH_TUPLES[tuple_idx];
    }

    return DEFAULT_TUPLE;
}

const FormatTuple& TextureRuntime::GetFormatTuple(VideoCore::CustomPixelFormat pixel_format) {
    const std::size_t format_index = static_cast<std::size_t>(pixel_format);
    return CUSTOM_TUPLES[format_index];
}

void TextureRuntime::Recycle(const HostTextureTag tag, Allocation&& alloc) {
    alloc_cache.emplace(tag, std::move(alloc));
}

Allocation TextureRuntime::Allocate(const VideoCore::SurfaceParams& params,
                                    const VideoCore::Material* material) {
    const GLenum target = params.texture_type == VideoCore::TextureType::CubeMap
                              ? GL_TEXTURE_CUBE_MAP
                              : GL_TEXTURE_2D;
    const bool is_custom = material != nullptr;
    const bool has_normal = material && material->Map(MapType::Normal);
    const auto& tuple =
        is_custom ? GetFormatTuple(params.custom_format) : GetFormatTuple(params.pixel_format);
    const HostTextureTag key = {
        .width = params.width,
        .height = params.height,
        .levels = params.levels,
        .res_scale = params.res_scale,
        .tuple = tuple,
        .type = params.texture_type,
        .is_custom = is_custom,
        .has_normal = has_normal,
    };

    if (auto it = alloc_cache.find(key); it != alloc_cache.end()) {
        auto alloc{std::move(it->second)};
        alloc_cache.erase(it);
        return alloc;
    }

    const GLuint old_tex = OpenGLState::GetCurState().texture_units[0].texture_2d;
    glActiveTexture(GL_TEXTURE0);

    std::array<OGLTexture, 3> textures{};
    std::array<GLuint, 3> handles{};

    textures[0] = MakeHandle(target, params.width, params.height, params.levels, tuple,
                             params.DebugName(false));
    handles.fill(textures[0].handle);

    if (params.res_scale != 1) {
        const u32 scaled_width = is_custom ? params.width : params.GetScaledWidth();
        const u32 scaled_height = is_custom ? params.height : params.GetScaledHeight();
        const auto& scaled_tuple = is_custom ? GetFormatTuple(PixelFormat::RGBA8) : tuple;
        textures[1] = MakeHandle(target, scaled_width, scaled_height, params.levels, scaled_tuple,
                                 params.DebugName(true, is_custom));
        handles[1] = textures[1].handle;
    }
    if (has_normal) {
        textures[2] = MakeHandle(target, params.width, params.height, params.levels, tuple,
                                 params.DebugName(true, is_custom));
        handles[2] = textures[2].handle;
    }

    glBindTexture(GL_TEXTURE_2D, old_tex);

    return Allocation{
        .textures = std::move(textures),
        .handles = std::move(handles),
        .tuple = tuple,
        .width = params.width,
        .height = params.height,
        .levels = params.levels,
        .res_scale = params.res_scale,
    };
}

bool TextureRuntime::ClearTexture(Surface& surface, const VideoCore::TextureClear& clear) {
    const auto prev_state = OpenGLState::GetCurState();

    // Setup scissor rectangle according to the clear rectangle
    OpenGLState state;
    state.scissor.enabled = true;
    state.scissor.x = clear.texture_rect.left;
    state.scissor.y = clear.texture_rect.bottom;
    state.scissor.width = clear.texture_rect.GetWidth();
    state.scissor.height = clear.texture_rect.GetHeight();
    state.draw.draw_framebuffer = draw_fbos[FboIndex(surface.type)].handle;
    state.Apply();

    switch (surface.type) {
    case SurfaceType::Color:
    case SurfaceType::Texture:
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               surface.Handle(), clear.texture_level);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0,
                               0);

        state.color_mask.red_enabled = true;
        state.color_mask.green_enabled = true;
        state.color_mask.blue_enabled = true;
        state.color_mask.alpha_enabled = true;
        state.Apply();

        glClearBufferfv(GL_COLOR, 0, clear.value.color.AsArray());
        break;
    case SurfaceType::Depth:
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                               surface.Handle(), clear.texture_level);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

        state.depth.write_mask = GL_TRUE;
        state.Apply();

        glClearBufferfv(GL_DEPTH, 0, &clear.value.depth);
        break;
    case SurfaceType::DepthStencil:
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                               surface.Handle(), clear.texture_level);

        state.depth.write_mask = GL_TRUE;
        state.stencil.write_mask = -1;
        state.Apply();

        glClearBufferfi(GL_DEPTH_STENCIL, 0, clear.value.depth, clear.value.stencil);
        break;
    default:
        UNREACHABLE_MSG("Unknown surface type {}", surface.type);
        return false;
    }

    prev_state.Apply();
    return true;
}

bool TextureRuntime::CopyTextures(Surface& source, Surface& dest,
                                  const VideoCore::TextureCopy& copy) {
    const GLenum src_textarget = source.texture_type == VideoCore::TextureType::CubeMap
                                     ? GL_TEXTURE_CUBE_MAP
                                     : GL_TEXTURE_2D;
    const GLenum dest_textarget =
        dest.texture_type == VideoCore::TextureType::CubeMap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
    glCopyImageSubData(source.Handle(), src_textarget, copy.src_level, copy.src_offset.x,
                       copy.src_offset.y, copy.src_layer, dest.Handle(), dest_textarget,
                       copy.dst_level, copy.dst_offset.x, copy.dst_offset.y, copy.dst_layer,
                       copy.extent.width, copy.extent.height, 1);
    return true;
}

bool TextureRuntime::BlitTextures(Surface& source, Surface& dest,
                                  const VideoCore::TextureBlit& blit) {
    OpenGLState state = OpenGLState::GetCurState();
    state.scissor.enabled = false;
    state.draw.read_framebuffer = read_fbos[FboIndex(source.type)].handle;
    state.draw.draw_framebuffer = draw_fbos[FboIndex(dest.type)].handle;
    state.Apply();

    source.Attach(GL_READ_FRAMEBUFFER, blit.src_level, blit.src_layer);
    dest.Attach(GL_DRAW_FRAMEBUFFER, blit.dst_level, blit.dst_layer);

    // TODO (wwylele): use GL_NEAREST for shadow map texture
    // Note: shadow map is treated as RGBA8 format in PICA, as well as in the rasterizer cache, but
    // doing linear intepolation componentwise would cause incorrect value. However, for a
    // well-programmed game this code path should be rarely executed for shadow map with
    // inconsistent scale.
    const GLbitfield buffer_mask = MakeBufferMask(source.type);
    const GLenum filter = buffer_mask == GL_COLOR_BUFFER_BIT ? GL_LINEAR : GL_NEAREST;
    glBlitFramebuffer(blit.src_rect.left, blit.src_rect.bottom, blit.src_rect.right,
                      blit.src_rect.top, blit.dst_rect.left, blit.dst_rect.bottom,
                      blit.dst_rect.right, blit.dst_rect.top, buffer_mask, filter);

    return true;
}

void TextureRuntime::GenerateMipmaps(Surface& surface) {
    OpenGLState state = OpenGLState::GetCurState();

    const auto generate = [&](u32 index) {
        state.texture_units[0].texture_2d = surface.Handle(index);
        state.Apply();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, surface.levels - 1);
        glGenerateMipmap(GL_TEXTURE_2D);
    };

    generate(1);
    if (surface.HasNormalMap()) {
        generate(2);
    }
}

const ReinterpreterList& TextureRuntime::GetPossibleReinterpretations(
    PixelFormat dest_format) const {
    return reinterpreters[static_cast<u32>(dest_format)];
}

Surface::Surface(TextureRuntime& runtime_, const VideoCore::SurfaceParams& params)
    : SurfaceBase{params}, driver{&runtime_.GetDriver()}, runtime{&runtime_} {
    if (pixel_format == PixelFormat::Invalid) {
        return;
    }

    alloc = runtime->Allocate(params);
}

Surface::~Surface() {
    if (pixel_format == PixelFormat::Invalid || !alloc) {
        return;
    }
    runtime->Recycle(MakeTag(), std::move(alloc));
}

void Surface::Upload(const VideoCore::BufferTextureCopy& upload,
                     const VideoCore::StagingData& staging) {
    ASSERT(stride * GetFormatBytesPerPixel(pixel_format) % 4 == 0);

    const GLuint old_tex = OpenGLState::GetCurState().texture_units[0].texture_2d;
    const u32 unscaled_width = upload.texture_rect.GetWidth();
    const u32 unscaled_height = upload.texture_rect.GetHeight();
    glPixelStorei(GL_UNPACK_ROW_LENGTH, unscaled_width);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, Handle(0));

    const auto& tuple = alloc.tuple;
    glTexSubImage2D(GL_TEXTURE_2D, upload.texture_level, upload.texture_rect.left,
                    upload.texture_rect.bottom, unscaled_width, unscaled_height, tuple.format,
                    tuple.type, staging.mapped.data());

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glBindTexture(GL_TEXTURE_2D, old_tex);

    const VideoCore::TextureBlit blit = {
        .src_level = upload.texture_level,
        .dst_level = upload.texture_level,
        .src_rect = upload.texture_rect,
        .dst_rect = upload.texture_rect * res_scale,
    };
    if (res_scale != 1 && !runtime->blit_helper.Filter(*this, blit)) {
        BlitScale(blit, true);
    }
}

void Surface::UploadCustom(const VideoCore::Material* material, u32 level) {
    const GLuint old_tex = OpenGLState::GetCurState().texture_units[0].texture_2d;
    const auto& tuple = alloc.tuple;
    const u32 width = material->width;
    const u32 height = material->height;
    const auto color = material->textures[0];
    const Common::Rectangle filter_rect{0U, height, width, 0U};

    glActiveTexture(GL_TEXTURE0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, width);

    glBindTexture(GL_TEXTURE_2D, Handle(0));
    if (VideoCore::IsCustomFormatCompressed(custom_format)) {
        const GLsizei image_size = static_cast<GLsizei>(color->data.size());
        glCompressedTexSubImage2D(GL_TEXTURE_2D, level, 0, 0, width, height, tuple.format,
                                  image_size, color->data.data());
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, level, 0, 0, width, height, tuple.format, tuple.type,
                        color->data.data());
    }

    const VideoCore::TextureBlit blit = {
        .src_rect = filter_rect,
        .dst_rect = filter_rect,
    };
    if (res_scale != 1 && !runtime->blit_helper.Filter(*this, blit)) {
        BlitScale(blit, true);
    }
    for (u32 i = 1; i < VideoCore::MAX_MAPS; i++) {
        const auto texture = material->textures[i];
        if (!texture) {
            continue;
        }
        glBindTexture(GL_TEXTURE_2D, Handle(i + 1));
        if (VideoCore::IsCustomFormatCompressed(custom_format)) {
            const GLsizei image_size = static_cast<GLsizei>(texture->data.size());
            glCompressedTexSubImage2D(GL_TEXTURE_2D, level, 0, 0, width, height, tuple.format,
                                      image_size, texture->data.data());
        } else {
            glTexSubImage2D(GL_TEXTURE_2D, level, 0, 0, width, height, tuple.format, tuple.type,
                            texture->data.data());
        }
    }

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glBindTexture(GL_TEXTURE_2D, old_tex);
}

void Surface::Download(const VideoCore::BufferTextureCopy& download,
                       const VideoCore::StagingData& staging) {
    ASSERT(stride * GetFormatBytesPerPixel(pixel_format) % 4 == 0);

    const u32 unscaled_width = download.texture_rect.GetWidth();
    const u32 unscaled_height = download.texture_rect.GetHeight();
    glPixelStorei(GL_UNPACK_ROW_LENGTH, unscaled_width);

    // Scale down upscaled data before downloading it
    if (res_scale != 1) {
        const VideoCore::TextureBlit blit = {
            .src_level = download.texture_level,
            .dst_level = download.texture_level,
            .src_rect = download.texture_rect * res_scale,
            .dst_rect = download.texture_rect,
        };
        BlitScale(blit, false);
    }

    // Try to download without using an fbo. This should succeed on recent desktop drivers
    if (DownloadWithoutFbo(download, staging)) {
        return;
    }

    OpenGLState state = OpenGLState::GetCurState();
    state.scissor.enabled = false;
    state.draw.read_framebuffer = runtime->read_fbos[FboIndex(type)].handle;
    state.Apply();

    Attach(GL_READ_FRAMEBUFFER, download.texture_level, 0, false);

    // Read the pixel data to the staging buffer
    const auto& tuple = runtime->GetFormatTuple(pixel_format);
    glReadPixels(download.texture_rect.left, download.texture_rect.bottom, unscaled_width,
                 unscaled_height, tuple.format, tuple.type, staging.mapped.data());

    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
}

bool Surface::DownloadWithoutFbo(const VideoCore::BufferTextureCopy& download,
                                 const VideoCore::StagingData& staging) {
    const bool is_full_download = download.texture_rect == GetRect();
    const bool has_sub_image = driver->HasArbGetTextureSubImage();
    if (driver->IsOpenGLES() || (!is_full_download && !has_sub_image)) {
        return false;
    }

    const GLuint old_tex = OpenGLState::GetCurState().texture_units[0].texture_2d;
    const auto& tuple = runtime->GetFormatTuple(pixel_format);

    glActiveTexture(GL_TEXTURE0);
    glPixelStorei(GL_PACK_ROW_LENGTH, static_cast<GLint>(stride));
    SCOPE_EXIT({ glPixelStorei(GL_PACK_ROW_LENGTH, 0); });

    // Prefer glGetTextureSubImage in most cases since it's the fastest and most convenient option
    if (has_sub_image) {
        const GLsizei buf_size = static_cast<GLsizei>(staging.mapped.size());
        glGetTextureSubImage(Handle(0), download.texture_level, download.texture_rect.left,
                             download.texture_rect.bottom, 0, download.texture_rect.GetWidth(),
                             download.texture_rect.GetHeight(), 1, tuple.format, tuple.type,
                             buf_size, staging.mapped.data());
        return true;
    }

    // This should only trigger for full texture downloads in oldish intel drivers
    // that only support up to 4.3
    glBindTexture(GL_TEXTURE_2D, Handle(0));
    glGetTexImage(GL_TEXTURE_2D, download.texture_level, tuple.format, tuple.type,
                  staging.mapped.data());
    glBindTexture(GL_TEXTURE_2D, old_tex);

    return true;
}

void Surface::Attach(GLenum target, u32 level, u32 layer, bool scaled) {
    const GLuint handle = Handle(static_cast<u32>(scaled));
    const GLenum textarget = texture_type == TextureType::CubeMap
                                 ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer
                                 : GL_TEXTURE_2D;

    switch (type) {
    case SurfaceType::Color:
    case SurfaceType::Texture:
        glFramebufferTexture2D(target, GL_COLOR_ATTACHMENT0, textarget, handle, level);
        break;
    case SurfaceType::Depth:
        glFramebufferTexture2D(target, GL_DEPTH_ATTACHMENT, textarget, handle, level);
        break;
    case SurfaceType::DepthStencil:
        glFramebufferTexture2D(target, GL_DEPTH_STENCIL_ATTACHMENT, textarget, handle, level);
        break;
    default:
        UNREACHABLE_MSG("Invalid surface type!");
    }
}

bool Surface::Swap(const VideoCore::Material* mat) {
    const VideoCore::CustomPixelFormat format{mat->format};
    if (!driver->IsCustomFormatSupported(format)) {
        return false;
    }
    runtime->Recycle(MakeTag(), std::move(alloc));

    SurfaceParams params = *this;
    params.width = mat->width;
    params.height = mat->height;
    params.custom_format = mat->format;
    alloc = runtime->Allocate(params, mat);

    LOG_DEBUG(Render_OpenGL, "Swapped {}x{} {} surface at address {:#x} to {}x{} {}",
              GetScaledWidth(), GetScaledHeight(), VideoCore::PixelFormatAsString(pixel_format),
              addr, width, height, VideoCore::CustomPixelFormatAsString(format));

    is_custom = true;
    custom_format = format;
    material = mat;

    return true;
}

u32 Surface::GetInternalBytesPerPixel() const {
    // RGB8 is converted to RGBA8 on OpenGL ES since it doesn't support BGR8
    if (driver->IsOpenGLES() && pixel_format == VideoCore::PixelFormat::RGB8) {
        return 4;
    }
    return GetFormatBytesPerPixel(pixel_format);
}

void Surface::BlitScale(const VideoCore::TextureBlit& blit, bool up_scale) {
    const u32 fbo_index = FboIndex(type);

    OpenGLState state = OpenGLState::GetCurState();
    state.scissor.enabled = false;
    state.draw.read_framebuffer = runtime->read_fbos[fbo_index].handle;
    state.draw.draw_framebuffer = runtime->draw_fbos[fbo_index].handle;
    state.Apply();

    Attach(GL_READ_FRAMEBUFFER, blit.src_level, blit.src_layer, !up_scale);
    Attach(GL_DRAW_FRAMEBUFFER, blit.dst_level, blit.dst_layer, up_scale);

    const GLenum buffer_mask = MakeBufferMask(type);
    const GLenum filter = buffer_mask == GL_COLOR_BUFFER_BIT ? GL_LINEAR : GL_NEAREST;
    glBlitFramebuffer(blit.src_rect.left, blit.src_rect.bottom, blit.src_rect.right,
                      blit.src_rect.top, blit.dst_rect.left, blit.dst_rect.bottom,
                      blit.dst_rect.right, blit.dst_rect.top, buffer_mask, filter);
}

HostTextureTag Surface::MakeTag() const noexcept {
    return HostTextureTag{
        .width = alloc.width,
        .height = alloc.height,
        .levels = alloc.levels,
        .res_scale = alloc.res_scale,
        .tuple = alloc.tuple,
        .type = texture_type,
        .is_custom = is_custom,
        .has_normal = HasNormalMap(),
    };
}

Framebuffer::Framebuffer(TextureRuntime& runtime, Surface* const color, u32 color_level,
                         Surface* const depth_stencil, u32 depth_level, const Pica::Regs& regs,
                         Common::Rectangle<u32> surfaces_rect)
    : VideoCore::FramebufferBase{regs,          color,       color_level,
                                 depth_stencil, depth_level, surfaces_rect} {

    const bool shadow_rendering = regs.framebuffer.IsShadowRendering();
    const bool has_stencil = regs.framebuffer.HasStencil();
    if (shadow_rendering && !color) {
        return;
    }

    if (color) {
        attachments[0] = color->Handle();
    }
    if (depth_stencil) {
        attachments[1] = depth_stencil->Handle();
    }

    const FramebufferInfo info = {
        .color = attachments[0],
        .depth = attachments[1],
        .color_level = color_level,
        .depth_level = depth_level,
    };

    const u64 hash = Common::ComputeHash64(&info, sizeof(FramebufferInfo));
    auto [it, new_framebuffer] = runtime.framebuffer_cache.try_emplace(hash);

    if (!new_framebuffer) {
        handle = it->second.handle;
        return;
    }

    const GLuint old_fbo = OpenGLState::GetCurState().draw.draw_framebuffer;

    OGLFramebuffer& framebuffer = it->second;
    framebuffer.Create();
    handle = it->second.handle;

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer.handle);
    SCOPE_EXIT({ glBindFramebuffer(GL_DRAW_FRAMEBUFFER, old_fbo); });

    if (shadow_rendering) {
        glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH,
                                color->width * res_scale);
        glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT,
                                color->height * res_scale);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0,
                               0);
    } else {
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               color ? color->Handle() : 0, color_level);
        if (depth_stencil) {
            if (has_stencil) {
                glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                       GL_TEXTURE_2D, depth_stencil->Handle(), depth_level);
            } else {
                glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                                       depth_stencil->Handle(), depth_level);
                glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0,
                                       0);
            }
        } else {
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                                   0, 0);
        }
    }
}

Framebuffer::~Framebuffer() = default;

} // namespace OpenGL
