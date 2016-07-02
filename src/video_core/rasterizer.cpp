// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cmath>

#include "common/assert.h"
#include "common/bit_field.h"
#include "common/color.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/math_util.h"
#include "common/microprofile.h"
#include "common/vector_math.h"

#include "core/memory.h"
#include "core/hw/gpu.h"

#include "video_core/debug_utils/debug_utils.h"
#include "video_core/pica.h"
#include "video_core/pica_state.h"
#include "video_core/pica_types.h"
#include "video_core/rasterizer.h"
#include "video_core/utils.h"
#include "video_core/shader/shader.h"

namespace Pica {

namespace Rasterizer {

static void DrawPixel(int x, int y, const Math::Vec4<u8>& color) {
    const auto& framebuffer = g_state.regs.framebuffer;
    const PAddr addr = framebuffer.GetColorBufferPhysicalAddress();

    // Similarly to textures, the render framebuffer is laid out from bottom to top, too.
    // NOTE: The framebuffer height register contains the actual FB height minus one.
    y = framebuffer.height - y;

    const u32 coarse_y = y & ~7;
    u32 bytes_per_pixel = GPU::Regs::BytesPerPixel(GPU::Regs::PixelFormat(framebuffer.color_format.Value()));
    u32 dst_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * framebuffer.width * bytes_per_pixel;
    u8* dst_pixel = Memory::GetPhysicalPointer(addr) + dst_offset;

    switch (framebuffer.color_format) {
    case Regs::ColorFormat::RGBA8:
        Color::EncodeRGBA8(color, dst_pixel);
        break;

    case Regs::ColorFormat::RGB8:
        Color::EncodeRGB8(color, dst_pixel);
        break;

    case Regs::ColorFormat::RGB5A1:
        Color::EncodeRGB5A1(color, dst_pixel);
        break;

    case Regs::ColorFormat::RGB565:
        Color::EncodeRGB565(color, dst_pixel);
        break;

    case Regs::ColorFormat::RGBA4:
        Color::EncodeRGBA4(color, dst_pixel);
        break;

    default:
        LOG_CRITICAL(Render_Software, "Unknown framebuffer color format %x", framebuffer.color_format.Value());
        UNIMPLEMENTED();
    }
}

static const Math::Vec4<u8> GetPixel(int x, int y) {
    const auto& framebuffer = g_state.regs.framebuffer;
    const PAddr addr = framebuffer.GetColorBufferPhysicalAddress();

    y = framebuffer.height - y;

    const u32 coarse_y = y & ~7;
    u32 bytes_per_pixel = GPU::Regs::BytesPerPixel(GPU::Regs::PixelFormat(framebuffer.color_format.Value()));
    u32 src_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * framebuffer.width * bytes_per_pixel;
    u8* src_pixel = Memory::GetPhysicalPointer(addr) + src_offset;

    switch (framebuffer.color_format) {
    case Regs::ColorFormat::RGBA8:
        return Color::DecodeRGBA8(src_pixel);

    case Regs::ColorFormat::RGB8:
        return Color::DecodeRGB8(src_pixel);

    case Regs::ColorFormat::RGB5A1:
        return Color::DecodeRGB5A1(src_pixel);

    case Regs::ColorFormat::RGB565:
        return Color::DecodeRGB565(src_pixel);

    case Regs::ColorFormat::RGBA4:
        return Color::DecodeRGBA4(src_pixel);

    default:
        LOG_CRITICAL(Render_Software, "Unknown framebuffer color format %x", framebuffer.color_format.Value());
        UNIMPLEMENTED();
    }

    return {0, 0, 0, 0};
}

static u32 GetDepth(int x, int y) {
    const auto& framebuffer = g_state.regs.framebuffer;
    const PAddr addr = framebuffer.GetDepthBufferPhysicalAddress();
    u8* depth_buffer = Memory::GetPhysicalPointer(addr);

    y = framebuffer.height - y;

    const u32 coarse_y = y & ~7;
    u32 bytes_per_pixel = Regs::BytesPerDepthPixel(framebuffer.depth_format);
    u32 stride = framebuffer.width * bytes_per_pixel;

    u32 src_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * stride;
    u8* src_pixel = depth_buffer + src_offset;

    switch (framebuffer.depth_format) {
        case Regs::DepthFormat::D16:
            return Color::DecodeD16(src_pixel);
        case Regs::DepthFormat::D24:
            return Color::DecodeD24(src_pixel);
        case Regs::DepthFormat::D24S8:
            return Color::DecodeD24S8(src_pixel).x;
        default:
            LOG_CRITICAL(HW_GPU, "Unimplemented depth format %u", framebuffer.depth_format);
            UNIMPLEMENTED();
            return 0;
    }
}

static u8 GetStencil(int x, int y) {
    const auto& framebuffer = g_state.regs.framebuffer;
    const PAddr addr = framebuffer.GetDepthBufferPhysicalAddress();
    u8* depth_buffer = Memory::GetPhysicalPointer(addr);

    y = framebuffer.height - y;

    const u32 coarse_y = y & ~7;
    u32 bytes_per_pixel = Pica::Regs::BytesPerDepthPixel(framebuffer.depth_format);
    u32 stride = framebuffer.width * bytes_per_pixel;

    u32 src_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * stride;
    u8* src_pixel = depth_buffer + src_offset;

    switch (framebuffer.depth_format) {
        case Regs::DepthFormat::D24S8:
            return Color::DecodeD24S8(src_pixel).y;

        default:
            LOG_WARNING(HW_GPU, "GetStencil called for function which doesn't have a stencil component (format %u)", framebuffer.depth_format);
            return 0;
    }
}

static void SetDepth(int x, int y, u32 value) {
    const auto& framebuffer = g_state.regs.framebuffer;
    const PAddr addr = framebuffer.GetDepthBufferPhysicalAddress();
    u8* depth_buffer = Memory::GetPhysicalPointer(addr);

    y = framebuffer.height - y;

    const u32 coarse_y = y & ~7;
    u32 bytes_per_pixel = Regs::BytesPerDepthPixel(framebuffer.depth_format);
    u32 stride = framebuffer.width * bytes_per_pixel;

    u32 dst_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * stride;
    u8* dst_pixel = depth_buffer + dst_offset;

    switch (framebuffer.depth_format) {
        case Regs::DepthFormat::D16:
            Color::EncodeD16(value, dst_pixel);
            break;

        case Regs::DepthFormat::D24:
            Color::EncodeD24(value, dst_pixel);
            break;

        case Regs::DepthFormat::D24S8:
            Color::EncodeD24X8(value, dst_pixel);
            break;

        default:
            LOG_CRITICAL(HW_GPU, "Unimplemented depth format %u", framebuffer.depth_format);
            UNIMPLEMENTED();
            break;
    }
}

static void SetStencil(int x, int y, u8 value) {
    const auto& framebuffer = g_state.regs.framebuffer;
    const PAddr addr = framebuffer.GetDepthBufferPhysicalAddress();
    u8* depth_buffer = Memory::GetPhysicalPointer(addr);

    y = framebuffer.height - y;

    const u32 coarse_y = y & ~7;
    u32 bytes_per_pixel = Pica::Regs::BytesPerDepthPixel(framebuffer.depth_format);
    u32 stride = framebuffer.width * bytes_per_pixel;

    u32 dst_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * stride;
    u8* dst_pixel = depth_buffer + dst_offset;

    switch (framebuffer.depth_format) {
        case Pica::Regs::DepthFormat::D16:
        case Pica::Regs::DepthFormat::D24:
            // Nothing to do
            break;

        case Pica::Regs::DepthFormat::D24S8:
            Color::EncodeX24S8(value, dst_pixel);
            break;

        default:
            LOG_CRITICAL(HW_GPU, "Unimplemented depth format %u", framebuffer.depth_format);
            UNIMPLEMENTED();
            break;
    }
}

static u8 PerformStencilAction(Regs::StencilAction action, u8 old_stencil, u8 ref) {
    switch (action) {
    case Regs::StencilAction::Keep:
        return old_stencil;

    case Regs::StencilAction::Zero:
        return 0;

    case Regs::StencilAction::Replace:
        return ref;

    case Regs::StencilAction::Increment:
        // Saturated increment
        return std::min<u8>(old_stencil, 254) + 1;

    case Regs::StencilAction::Decrement:
        // Saturated decrement
        return std::max<u8>(old_stencil, 1) - 1;

    case Regs::StencilAction::Invert:
        return ~old_stencil;

    case Regs::StencilAction::IncrementWrap:
        return old_stencil + 1;

    case Regs::StencilAction::DecrementWrap:
        return old_stencil - 1;

    default:
        LOG_CRITICAL(HW_GPU, "Unknown stencil action %x", (int)action);
        UNIMPLEMENTED();
        return 0;
    }
}

// NOTE: Assuming that rasterizer coordinates are 12.4 fixed-point values
struct Fix12P4 {
    Fix12P4() {}
    Fix12P4(u16 val) : val(val) {}

    static u16 FracMask() { return 0xF; }
    static u16 IntMask() { return (u16)~0xF; }

    operator u16() const {
        return val;
    }

    bool operator < (const Fix12P4& oth) const {
        return (u16)*this < (u16)oth;
    }

private:
    u16 val;
};

/**
 * Calculate signed area of the triangle spanned by the three argument vertices.
 * The sign denotes an orientation.
 *
 * @todo define orientation concretely.
 */
static int SignedArea (const Math::Vec2<Fix12P4>& vtx1,
                       const Math::Vec2<Fix12P4>& vtx2,
                       const Math::Vec2<Fix12P4>& vtx3) {
    const auto vec1 = Math::MakeVec(vtx2 - vtx1, 0);
    const auto vec2 = Math::MakeVec(vtx3 - vtx1, 0);
    // TODO: There is a very small chance this will overflow for sizeof(int) == 4
    return Math::Cross(vec1, vec2).z;
};

MICROPROFILE_DEFINE(GPU_Rasterization, "GPU", "Rasterization", MP_RGB(50, 50, 240));

/**
 * Helper function for ProcessTriangle with the "reversed" flag to allow for implementing
 * culling via recursion.
 */
static void ProcessTriangleInternal(const Shader::OutputVertex& v0,
                                    const Shader::OutputVertex& v1,
                                    const Shader::OutputVertex& v2,
                                    bool reversed = false)
{
    const auto& regs = g_state.regs;
    MICROPROFILE_SCOPE(GPU_Rasterization);

    // vertex positions in rasterizer coordinates
    static auto FloatToFix = [](float24 flt) {
        // TODO: Rounding here is necessary to prevent garbage pixels at
        //       triangle borders. Is it that the correct solution, though?
        return Fix12P4(static_cast<unsigned short>(round(flt.ToFloat32() * 16.0f)));
    };
    static auto ScreenToRasterizerCoordinates = [](const Math::Vec3<float24>& vec) {
        return Math::Vec3<Fix12P4>{FloatToFix(vec.x), FloatToFix(vec.y), FloatToFix(vec.z)};
    };

    Math::Vec3<Fix12P4> vtxpos[3]{ ScreenToRasterizerCoordinates(v0.screenpos),
                                   ScreenToRasterizerCoordinates(v1.screenpos),
                                   ScreenToRasterizerCoordinates(v2.screenpos) };

    if (regs.cull_mode == Regs::CullMode::KeepAll) {
        // Make sure we always end up with a triangle wound counter-clockwise
        if (!reversed && SignedArea(vtxpos[0].xy(), vtxpos[1].xy(), vtxpos[2].xy()) <= 0) {
            ProcessTriangleInternal(v0, v2, v1, true);
            return;
        }
    } else {
        if (!reversed && regs.cull_mode == Regs::CullMode::KeepClockWise) {
            // Reverse vertex order and use the CCW code path.
            ProcessTriangleInternal(v0, v2, v1, true);
            return;
        }

        // Cull away triangles which are wound clockwise.
        if (SignedArea(vtxpos[0].xy(), vtxpos[1].xy(), vtxpos[2].xy()) <= 0)
            return;
    }

    u16 min_x = std::min({vtxpos[0].x, vtxpos[1].x, vtxpos[2].x});
    u16 min_y = std::min({vtxpos[0].y, vtxpos[1].y, vtxpos[2].y});
    u16 max_x = std::max({vtxpos[0].x, vtxpos[1].x, vtxpos[2].x});
    u16 max_y = std::max({vtxpos[0].y, vtxpos[1].y, vtxpos[2].y});

    // Convert the scissor box coordinates to 12.4 fixed point
    u16 scissor_x1 = (u16)( regs.scissor_test.x1      << 4);
    u16 scissor_y1 = (u16)( regs.scissor_test.y1      << 4);
    // x2,y2 have +1 added to cover the entire sub-pixel area
    u16 scissor_x2 = (u16)((regs.scissor_test.x2 + 1) << 4);
    u16 scissor_y2 = (u16)((regs.scissor_test.y2 + 1) << 4);

    if (regs.scissor_test.mode == Regs::ScissorMode::Include) {
        // Calculate the new bounds
        min_x = std::max(min_x, scissor_x1);
        min_y = std::max(min_y, scissor_y1);
        max_x = std::min(max_x, scissor_x2);
        max_y = std::min(max_y, scissor_y2);
    }

    min_x &= Fix12P4::IntMask();
    min_y &= Fix12P4::IntMask();
    max_x = ((max_x + Fix12P4::FracMask()) & Fix12P4::IntMask());
    max_y = ((max_y + Fix12P4::FracMask()) & Fix12P4::IntMask());

    // Triangle filling rules: Pixels on the right-sided edge or on flat bottom edges are not
    // drawn. Pixels on any other triangle border are drawn. This is implemented with three bias
    // values which are added to the barycentric coordinates w0, w1 and w2, respectively.
    // NOTE: These are the PSP filling rules. Not sure if the 3DS uses the same ones...
    auto IsRightSideOrFlatBottomEdge = [](const Math::Vec2<Fix12P4>& vtx,
                                          const Math::Vec2<Fix12P4>& line1,
                                          const Math::Vec2<Fix12P4>& line2)
    {
        if (line1.y == line2.y) {
            // just check if vertex is above us => bottom line parallel to x-axis
            return vtx.y < line1.y;
        } else {
            // check if vertex is on our left => right side
            // TODO: Not sure how likely this is to overflow
            return (int)vtx.x < (int)line1.x + ((int)line2.x - (int)line1.x) * ((int)vtx.y - (int)line1.y) / ((int)line2.y - (int)line1.y);
        }
    };
    int bias0 = IsRightSideOrFlatBottomEdge(vtxpos[0].xy(), vtxpos[1].xy(), vtxpos[2].xy()) ? -1 : 0;
    int bias1 = IsRightSideOrFlatBottomEdge(vtxpos[1].xy(), vtxpos[2].xy(), vtxpos[0].xy()) ? -1 : 0;
    int bias2 = IsRightSideOrFlatBottomEdge(vtxpos[2].xy(), vtxpos[0].xy(), vtxpos[1].xy()) ? -1 : 0;

    auto w_inverse = Math::MakeVec(v0.pos.w, v1.pos.w, v2.pos.w);

    auto textures = regs.GetTextures();
    auto tev_stages = regs.GetTevStages();

    bool stencil_action_enable = g_state.regs.output_merger.stencil_test.enable && g_state.regs.framebuffer.depth_format == Regs::DepthFormat::D24S8;
    const auto stencil_test = g_state.regs.output_merger.stencil_test;

    // Enter rasterization loop, starting at the center of the topleft bounding box corner.
    // TODO: Not sure if looping through x first might be faster
    for (u16 y = min_y + 8; y < max_y; y += 0x10) {
        for (u16 x = min_x + 8; x < max_x; x += 0x10) {

            // Do not process the pixel if it's inside the scissor box and the scissor mode is set to Exclude
            if (regs.scissor_test.mode == Regs::ScissorMode::Exclude) {
                if (x >= scissor_x1 && x < scissor_x2 &&
                    y >= scissor_y1 && y < scissor_y2)
                    continue;
            }

            // Calculate the barycentric coordinates w0, w1 and w2
            int w0 = bias0 + SignedArea(vtxpos[1].xy(), vtxpos[2].xy(), {x, y});
            int w1 = bias1 + SignedArea(vtxpos[2].xy(), vtxpos[0].xy(), {x, y});
            int w2 = bias2 + SignedArea(vtxpos[0].xy(), vtxpos[1].xy(), {x, y});
            int wsum = w0 + w1 + w2;

            // If current pixel is not covered by the current primitive
            if (w0 < 0 || w1 < 0 || w2 < 0)
                continue;

            auto baricentric_coordinates = Math::MakeVec(float24::FromFloat32(static_cast<float>(w0)),
                                                float24::FromFloat32(static_cast<float>(w1)),
                                                float24::FromFloat32(static_cast<float>(w2)));
            float24 interpolated_w_inverse = float24::FromFloat32(1.0f) / Math::Dot(w_inverse, baricentric_coordinates);

            // interpolated_z = z / w
            float interpolated_z_over_w = (v0.screenpos[2].ToFloat32() * w0 +
                                           v1.screenpos[2].ToFloat32() * w1 +
                                           v2.screenpos[2].ToFloat32() * w2) / wsum;

            // Not fully accurate. About 3 bits in precision are missing.
            // Z-Buffer (z / w * scale + offset)
            float depth_scale = float24::FromRaw(regs.viewport_depth_range).ToFloat32();
            float depth_offset = float24::FromRaw(regs.viewport_depth_near_plane).ToFloat32();
            float depth = interpolated_z_over_w * depth_scale + depth_offset;

            // Potentially switch to W-Buffer
            if (regs.depthmap_enable == Pica::Regs::DepthBuffering::WBuffering) {
                // W-Buffer (z * scale + w * offset = (z / w * scale + offset) * w)
                depth *= interpolated_w_inverse.ToFloat32() * wsum;
            }

            // Clamp the result
            depth = MathUtil::Clamp(depth, 0.0f, 1.0f);

            // Perspective correct attribute interpolation:
            // Attribute values cannot be calculated by simple linear interpolation since
            // they are not linear in screen space. For example, when interpolating a
            // texture coordinate across two vertices, something simple like
            //     u = (u0*w0 + u1*w1)/(w0+w1)
            // will not work. However, the attribute value divided by the
            // clipspace w-coordinate (u/w) and and the inverse w-coordinate (1/w) are linear
            // in screenspace. Hence, we can linearly interpolate these two independently and
            // calculate the interpolated attribute by dividing the results.
            // I.e.
            //     u_over_w   = ((u0/v0.pos.w)*w0 + (u1/v1.pos.w)*w1)/(w0+w1)
            //     one_over_w = (( 1/v0.pos.w)*w0 + ( 1/v1.pos.w)*w1)/(w0+w1)
            //     u = u_over_w / one_over_w
            //
            // The generalization to three vertices is straightforward in baricentric coordinates.
            auto GetInterpolatedAttribute = [&](float24 attr0, float24 attr1, float24 attr2) {
                auto attr_over_w = Math::MakeVec(attr0, attr1, attr2);
                float24 interpolated_attr_over_w = Math::Dot(attr_over_w, baricentric_coordinates);
                return interpolated_attr_over_w * interpolated_w_inverse;
            };

            Math::Vec4<u8> primary_color{
                (u8)(GetInterpolatedAttribute(v0.color.r(), v1.color.r(), v2.color.r()).ToFloat32() * 255),
                (u8)(GetInterpolatedAttribute(v0.color.g(), v1.color.g(), v2.color.g()).ToFloat32() * 255),
                (u8)(GetInterpolatedAttribute(v0.color.b(), v1.color.b(), v2.color.b()).ToFloat32() * 255),
                (u8)(GetInterpolatedAttribute(v0.color.a(), v1.color.a(), v2.color.a()).ToFloat32() * 255)
            };

            Math::Vec2<float24> uv[3];
            uv[0].u() = GetInterpolatedAttribute(v0.tc0.u(), v1.tc0.u(), v2.tc0.u());
            uv[0].v() = GetInterpolatedAttribute(v0.tc0.v(), v1.tc0.v(), v2.tc0.v());
            uv[1].u() = GetInterpolatedAttribute(v0.tc1.u(), v1.tc1.u(), v2.tc1.u());
            uv[1].v() = GetInterpolatedAttribute(v0.tc1.v(), v1.tc1.v(), v2.tc1.v());
            uv[2].u() = GetInterpolatedAttribute(v0.tc2.u(), v1.tc2.u(), v2.tc2.u());
            uv[2].v() = GetInterpolatedAttribute(v0.tc2.v(), v1.tc2.v(), v2.tc2.v());

            Math::Vec4<u8> texture_color[3]{};
            for (int i = 0; i < 3; ++i) {
                const auto& texture = textures[i];
                if (!texture.enabled)
                    continue;

                DEBUG_ASSERT(0 != texture.config.address);

                float24 u = uv[i].u();
                float24 v = uv[i].v();

                // Only unit 0 respects the texturing type (according to 3DBrew)
                // TODO: Refactor so cubemaps and shadowmaps can be handled
                if (i == 0) {
                    switch(texture.config.type) {
                    case Regs::TextureConfig::Texture2D:
                        break;
                    case Regs::TextureConfig::Projection2D: {
                        auto tc0_w = GetInterpolatedAttribute(v0.tc0_w, v1.tc0_w, v2.tc0_w);
                        u /= tc0_w;
                        v /= tc0_w;
                        break;
                    }
                    default:
                        // TODO: Change to LOG_ERROR when more types are handled.
                        LOG_DEBUG(HW_GPU, "Unhandled texture type %x", (int)texture.config.type);
                        UNIMPLEMENTED();
                        break;
                    }
                }

                int s = (int)(u * float24::FromFloat32(static_cast<float>(texture.config.width))).ToFloat32();
                int t = (int)(v * float24::FromFloat32(static_cast<float>(texture.config.height))).ToFloat32();


                static auto GetWrappedTexCoord = [](Regs::TextureConfig::WrapMode mode, int val, unsigned size) {
                    switch (mode) {
                        case Regs::TextureConfig::ClampToEdge:
                            val = std::max(val, 0);
                            val = std::min(val, (int)size - 1);
                            return val;

                        case Regs::TextureConfig::ClampToBorder:
                            return val;

                        case Regs::TextureConfig::Repeat:
                            return (int)((unsigned)val % size);

                        case Regs::TextureConfig::MirroredRepeat:
                        {
                            unsigned int coord = ((unsigned)val % (2 * size));
                            if (coord >= size)
                                coord = 2 * size - 1 - coord;
                            return (int)coord;
                        }

                        default:
                            LOG_ERROR(HW_GPU, "Unknown texture coordinate wrapping mode %x", (int)mode);
                            UNIMPLEMENTED();
                            return 0;
                    }
                };

                if ((texture.config.wrap_s == Regs::TextureConfig::ClampToBorder && (s < 0 || s >= texture.config.width))
                    || (texture.config.wrap_t == Regs::TextureConfig::ClampToBorder && (t < 0 || t >= texture.config.height))) {
                    auto border_color = texture.config.border_color;
                    texture_color[i] = { border_color.r, border_color.g, border_color.b, border_color.a };
                } else {
                    // Textures are laid out from bottom to top, hence we invert the t coordinate.
                    // NOTE: This may not be the right place for the inversion.
                    // TODO: Check if this applies to ETC textures, too.
                    s = GetWrappedTexCoord(texture.config.wrap_s, s, texture.config.width);
                    t = texture.config.height - 1 - GetWrappedTexCoord(texture.config.wrap_t, t, texture.config.height);

                    u8* texture_data = Memory::GetPhysicalPointer(texture.config.GetPhysicalAddress());
                    auto info = DebugUtils::TextureInfo::FromPicaRegister(texture.config, texture.format);

                    // TODO: Apply the min and mag filters to the texture
                    texture_color[i] = DebugUtils::LookupTexture(texture_data, s, t, info);
#if PICA_DUMP_TEXTURES
                    DebugUtils::DumpTexture(texture.config, texture_data);
#endif
                }
            }

            // Texture environment - consists of 6 stages of color and alpha combining.
            //
            // Color combiners take three input color values from some source (e.g. interpolated
            // vertex color, texture color, previous stage, etc), perform some very simple
            // operations on each of them (e.g. inversion) and then calculate the output color
            // with some basic arithmetic. Alpha combiners can be configured separately but work
            // analogously.
            Math::Vec4<u8> combiner_output;
            Math::Vec4<u8> combiner_buffer = {0, 0, 0, 0};
            Math::Vec4<u8> next_combiner_buffer = {
                regs.tev_combiner_buffer_color.r, regs.tev_combiner_buffer_color.g,
                regs.tev_combiner_buffer_color.b, regs.tev_combiner_buffer_color.a
            };

            for (unsigned tev_stage_index = 0; tev_stage_index < tev_stages.size(); ++tev_stage_index) {
                const auto& tev_stage = tev_stages[tev_stage_index];
                using Source = Regs::TevStageConfig::Source;
                using ColorModifier = Regs::TevStageConfig::ColorModifier;
                using AlphaModifier = Regs::TevStageConfig::AlphaModifier;
                using Operation = Regs::TevStageConfig::Operation;

                auto GetSource = [&](Source source) -> Math::Vec4<u8> {
                    switch (source) {
                    case Source::PrimaryColor:

                    // HACK: Until we implement fragment lighting, use primary_color
                    case Source::PrimaryFragmentColor:
                        return primary_color;

                    // HACK: Until we implement fragment lighting, use zero
                    case Source::SecondaryFragmentColor:
                        return {0, 0, 0, 0};

                    case Source::Texture0:
                        return texture_color[0];

                    case Source::Texture1:
                        return texture_color[1];

                    case Source::Texture2:
                        return texture_color[2];

                    case Source::PreviousBuffer:
                        return combiner_buffer;

                    case Source::Constant:
                        return {tev_stage.const_r, tev_stage.const_g, tev_stage.const_b, tev_stage.const_a};

                    case Source::Previous:
                        return combiner_output;

                    default:
                        LOG_ERROR(HW_GPU, "Unknown color combiner source %d", (int)source);
                        UNIMPLEMENTED();
                        return {0, 0, 0, 0};
                    }
                };

                static auto GetColorModifier = [](ColorModifier factor, const Math::Vec4<u8>& values) -> Math::Vec3<u8> {
                    switch (factor) {
                    case ColorModifier::SourceColor:
                        return values.rgb();

                    case ColorModifier::OneMinusSourceColor:
                        return (Math::Vec3<u8>(255, 255, 255) - values.rgb()).Cast<u8>();

                    case ColorModifier::SourceAlpha:
                        return values.aaa();

                    case ColorModifier::OneMinusSourceAlpha:
                        return (Math::Vec3<u8>(255, 255, 255) - values.aaa()).Cast<u8>();

                    case ColorModifier::SourceRed:
                        return values.rrr();

                    case ColorModifier::OneMinusSourceRed:
                        return (Math::Vec3<u8>(255, 255, 255) - values.rrr()).Cast<u8>();

                    case ColorModifier::SourceGreen:
                        return values.ggg();

                    case ColorModifier::OneMinusSourceGreen:
                        return (Math::Vec3<u8>(255, 255, 255) - values.ggg()).Cast<u8>();

                    case ColorModifier::SourceBlue:
                        return values.bbb();

                    case ColorModifier::OneMinusSourceBlue:
                        return (Math::Vec3<u8>(255, 255, 255) - values.bbb()).Cast<u8>();
                    }
                };

                static auto GetAlphaModifier = [](AlphaModifier factor, const Math::Vec4<u8>& values) -> u8 {
                    switch (factor) {
                    case AlphaModifier::SourceAlpha:
                        return values.a();

                    case AlphaModifier::OneMinusSourceAlpha:
                        return 255 - values.a();

                    case AlphaModifier::SourceRed:
                        return values.r();

                    case AlphaModifier::OneMinusSourceRed:
                        return 255 - values.r();

                    case AlphaModifier::SourceGreen:
                        return values.g();

                    case AlphaModifier::OneMinusSourceGreen:
                        return 255 - values.g();

                    case AlphaModifier::SourceBlue:
                        return values.b();

                    case AlphaModifier::OneMinusSourceBlue:
                        return 255 - values.b();
                    }
                };

                static auto ColorCombine = [](Operation op, const Math::Vec3<u8> input[3]) -> Math::Vec3<u8> {
                    switch (op) {
                    case Operation::Replace:
                        return input[0];

                    case Operation::Modulate:
                        return ((input[0] * input[1]) / 255).Cast<u8>();

                    case Operation::Add:
                    {
                        auto result = input[0] + input[1];
                        result.r() = std::min(255, result.r());
                        result.g() = std::min(255, result.g());
                        result.b() = std::min(255, result.b());
                        return result.Cast<u8>();
                    }

                    case Operation::AddSigned:
                    {
                        // TODO(bunnei): Verify that the color conversion from (float) 0.5f to (byte) 128 is correct
                        auto result = input[0].Cast<int>() + input[1].Cast<int>() - Math::MakeVec<int>(128, 128, 128);
                        result.r() = MathUtil::Clamp<int>(result.r(), 0, 255);
                        result.g() = MathUtil::Clamp<int>(result.g(), 0, 255);
                        result.b() = MathUtil::Clamp<int>(result.b(), 0, 255);
                        return result.Cast<u8>();
                    }

                    case Operation::Lerp:
                        return ((input[0] * input[2] + input[1] * (Math::MakeVec<u8>(255, 255, 255) - input[2]).Cast<u8>()) / 255).Cast<u8>();

                    case Operation::Subtract:
                    {
                        auto result = input[0].Cast<int>() - input[1].Cast<int>();
                        result.r() = std::max(0, result.r());
                        result.g() = std::max(0, result.g());
                        result.b() = std::max(0, result.b());
                        return result.Cast<u8>();
                    }

                    case Operation::MultiplyThenAdd:
                    {
                        auto result = (input[0] * input[1] + 255 * input[2].Cast<int>()) / 255;
                        result.r() = std::min(255, result.r());
                        result.g() = std::min(255, result.g());
                        result.b() = std::min(255, result.b());
                        return result.Cast<u8>();
                    }

                    case Operation::AddThenMultiply:
                    {
                        auto result = input[0] + input[1];
                        result.r() = std::min(255, result.r());
                        result.g() = std::min(255, result.g());
                        result.b() = std::min(255, result.b());
                        result = (result * input[2].Cast<int>()) / 255;
                        return result.Cast<u8>();
                    }
                    case Operation::Dot3_RGB:
                    {
                        // Not fully accurate.
                        // Worst case scenario seems to yield a +/-3 error
                        // Some HW results indicate that the per-component computation can't have a higher precision than 1/256,
                        // while dot3_rgb( (0x80,g0,b0),(0x7F,g1,b1) ) and dot3_rgb( (0x80,g0,b0),(0x80,g1,b1) ) give different results
                        int result = ((input[0].r() * 2 - 255) * (input[1].r() * 2 - 255) + 128) / 256 +
                                     ((input[0].g() * 2 - 255) * (input[1].g() * 2 - 255) + 128) / 256 +
                                     ((input[0].b() * 2 - 255) * (input[1].b() * 2 - 255) + 128) / 256;
                        result = std::max(0, std::min(255, result));
                        return { (u8)result, (u8)result, (u8)result };
                    }
                    default:
                        LOG_ERROR(HW_GPU, "Unknown color combiner operation %d", (int)op);
                        UNIMPLEMENTED();
                        return {0, 0, 0};
                    }
                };

                static auto AlphaCombine = [](Operation op, const std::array<u8,3>& input) -> u8 {
                    switch (op) {
                    case Operation::Replace:
                        return input[0];

                    case Operation::Modulate:
                        return input[0] * input[1] / 255;

                    case Operation::Add:
                        return std::min(255, input[0] + input[1]);

                    case Operation::AddSigned:
                    {
                        // TODO(bunnei): Verify that the color conversion from (float) 0.5f to (byte) 128 is correct
                        auto result = static_cast<int>(input[0]) + static_cast<int>(input[1]) - 128;
                        return static_cast<u8>(MathUtil::Clamp<int>(result, 0, 255));
                    }

                    case Operation::Lerp:
                        return (input[0] * input[2] + input[1] * (255 - input[2])) / 255;

                    case Operation::Subtract:
                        return std::max(0, (int)input[0] - (int)input[1]);

                    case Operation::MultiplyThenAdd:
                        return std::min(255, (input[0] * input[1] + 255 * input[2]) / 255);

                    case Operation::AddThenMultiply:
                        return (std::min(255, (input[0] + input[1])) * input[2]) / 255;

                    default:
                        LOG_ERROR(HW_GPU, "Unknown alpha combiner operation %d", (int)op);
                        UNIMPLEMENTED();
                        return 0;
                    }
                };

                // color combiner
                // NOTE: Not sure if the alpha combiner might use the color output of the previous
                //       stage as input. Hence, we currently don't directly write the result to
                //       combiner_output.rgb(), but instead store it in a temporary variable until
                //       alpha combining has been done.
                Math::Vec3<u8> color_result[3] = {
                    GetColorModifier(tev_stage.color_modifier1, GetSource(tev_stage.color_source1)),
                    GetColorModifier(tev_stage.color_modifier2, GetSource(tev_stage.color_source2)),
                    GetColorModifier(tev_stage.color_modifier3, GetSource(tev_stage.color_source3))
                };
                auto color_output = ColorCombine(tev_stage.color_op, color_result);

                // alpha combiner
                std::array<u8,3> alpha_result = {{
                    GetAlphaModifier(tev_stage.alpha_modifier1, GetSource(tev_stage.alpha_source1)),
                    GetAlphaModifier(tev_stage.alpha_modifier2, GetSource(tev_stage.alpha_source2)),
                    GetAlphaModifier(tev_stage.alpha_modifier3, GetSource(tev_stage.alpha_source3))
                }};
                auto alpha_output = AlphaCombine(tev_stage.alpha_op, alpha_result);

                combiner_output[0] = std::min((unsigned)255, color_output.r() * tev_stage.GetColorMultiplier());
                combiner_output[1] = std::min((unsigned)255, color_output.g() * tev_stage.GetColorMultiplier());
                combiner_output[2] = std::min((unsigned)255, color_output.b() * tev_stage.GetColorMultiplier());
                combiner_output[3] = std::min((unsigned)255, alpha_output * tev_stage.GetAlphaMultiplier());

                combiner_buffer = next_combiner_buffer;

                if (regs.tev_combiner_buffer_input.TevStageUpdatesCombinerBufferColor(tev_stage_index)) {
                    next_combiner_buffer.r() = combiner_output.r();
                    next_combiner_buffer.g() = combiner_output.g();
                    next_combiner_buffer.b() = combiner_output.b();
                }

                if (regs.tev_combiner_buffer_input.TevStageUpdatesCombinerBufferAlpha(tev_stage_index)) {
                    next_combiner_buffer.a() = combiner_output.a();
                }
            }

            const auto& output_merger = regs.output_merger;
            // TODO: Does alpha testing happen before or after stencil?
            if (output_merger.alpha_test.enable) {
                bool pass = false;

                switch (output_merger.alpha_test.func) {
                case Regs::CompareFunc::Never:
                    pass = false;
                    break;

                case Regs::CompareFunc::Always:
                    pass = true;
                    break;

                case Regs::CompareFunc::Equal:
                    pass = combiner_output.a() == output_merger.alpha_test.ref;
                    break;

                case Regs::CompareFunc::NotEqual:
                    pass = combiner_output.a() != output_merger.alpha_test.ref;
                    break;

                case Regs::CompareFunc::LessThan:
                    pass = combiner_output.a() < output_merger.alpha_test.ref;
                    break;

                case Regs::CompareFunc::LessThanOrEqual:
                    pass = combiner_output.a() <= output_merger.alpha_test.ref;
                    break;

                case Regs::CompareFunc::GreaterThan:
                    pass = combiner_output.a() > output_merger.alpha_test.ref;
                    break;

                case Regs::CompareFunc::GreaterThanOrEqual:
                    pass = combiner_output.a() >= output_merger.alpha_test.ref;
                    break;
                }

                if (!pass)
                    continue;
            }

            // Apply fog combiner
            // Not fully accurate. We'd have to know what data type is used to
            // store the depth etc. Using float for now until we know more
            // about Pica datatypes
            if (regs.fog_mode == Regs::FogMode::Fog) {
                const Math::Vec3<u8> fog_color = {
                    static_cast<u8>(regs.fog_color.r.Value()),
                    static_cast<u8>(regs.fog_color.g.Value()),
                    static_cast<u8>(regs.fog_color.b.Value()),
                };

                // Get index into fog LUT
                float fog_index;
                if (g_state.regs.fog_flip) {
                    fog_index = (1.0f - depth) * 128.0f;
                } else {
                    fog_index = depth * 128.0f;
                }

                // Generate clamped fog factor from LUT for given fog index
                float fog_i = MathUtil::Clamp(floorf(fog_index), 0.0f, 127.0f);
                float fog_f = fog_index - fog_i;
                const auto& fog_lut_entry = g_state.fog.lut[static_cast<unsigned int>(fog_i)];
                float fog_factor = (fog_lut_entry.value + fog_lut_entry.difference * fog_f) / 2047.0f; // This is signed fixed point 1.11
                fog_factor = MathUtil::Clamp(fog_factor, 0.0f, 1.0f);

                // Blend the fog
                for (unsigned i = 0; i < 3; i++) {
                    combiner_output[i] = fog_factor * combiner_output[i] + (1.0f - fog_factor) * fog_color[i];
                }
            }

            u8 old_stencil = 0;

            auto UpdateStencil = [stencil_test, x, y, &old_stencil](Pica::Regs::StencilAction action) {
                u8 new_stencil = PerformStencilAction(action, old_stencil, stencil_test.reference_value);
                if (g_state.regs.framebuffer.allow_depth_stencil_write != 0)
                    SetStencil(x >> 4, y >> 4, (new_stencil & stencil_test.write_mask) | (old_stencil & ~stencil_test.write_mask));
            };

            if (stencil_action_enable) {
                old_stencil = GetStencil(x >> 4, y >> 4);
                u8 dest = old_stencil & stencil_test.input_mask;
                u8 ref = stencil_test.reference_value & stencil_test.input_mask;

                bool pass = false;
                switch (stencil_test.func) {
                case Regs::CompareFunc::Never:
                    pass = false;
                    break;

                case Regs::CompareFunc::Always:
                    pass = true;
                    break;

                case Regs::CompareFunc::Equal:
                    pass = (ref == dest);
                    break;

                case Regs::CompareFunc::NotEqual:
                    pass = (ref != dest);
                    break;

                case Regs::CompareFunc::LessThan:
                    pass = (ref < dest);
                    break;

                case Regs::CompareFunc::LessThanOrEqual:
                    pass = (ref <= dest);
                    break;

                case Regs::CompareFunc::GreaterThan:
                    pass = (ref > dest);
                    break;

                case Regs::CompareFunc::GreaterThanOrEqual:
                    pass = (ref >= dest);
                    break;
                }

                if (!pass) {
                    UpdateStencil(stencil_test.action_stencil_fail);
                    continue;
                }
            }

            // Convert float to integer
            unsigned num_bits = Regs::DepthBitsPerPixel(regs.framebuffer.depth_format);
            u32 z = (u32)(depth * ((1 << num_bits) - 1));

            if (output_merger.depth_test_enable) {
                u32 ref_z = GetDepth(x >> 4, y >> 4);

                bool pass = false;

                switch (output_merger.depth_test_func) {
                case Regs::CompareFunc::Never:
                    pass = false;
                    break;

                case Regs::CompareFunc::Always:
                    pass = true;
                    break;

                case Regs::CompareFunc::Equal:
                    pass = z == ref_z;
                    break;

                case Regs::CompareFunc::NotEqual:
                    pass = z != ref_z;
                    break;

                case Regs::CompareFunc::LessThan:
                    pass = z < ref_z;
                    break;

                case Regs::CompareFunc::LessThanOrEqual:
                    pass = z <= ref_z;
                    break;

                case Regs::CompareFunc::GreaterThan:
                    pass = z > ref_z;
                    break;

                case Regs::CompareFunc::GreaterThanOrEqual:
                    pass = z >= ref_z;
                    break;
                }

                if (!pass) {
                    if (stencil_action_enable)
                        UpdateStencil(stencil_test.action_depth_fail);
                    continue;
                }
            }

            if (regs.framebuffer.allow_depth_stencil_write != 0 && output_merger.depth_write_enable)
                SetDepth(x >> 4, y >> 4, z);

            // The stencil depth_pass action is executed even if depth testing is disabled
            if (stencil_action_enable)
                UpdateStencil(stencil_test.action_depth_pass);

            auto dest = GetPixel(x >> 4, y >> 4);
            Math::Vec4<u8> blend_output = combiner_output;

            if (output_merger.alphablend_enable) {
                auto params = output_merger.alpha_blending;

                auto LookupFactor = [&](unsigned channel, Regs::BlendFactor factor) -> u8 {
                    DEBUG_ASSERT(channel < 4);

                    const Math::Vec4<u8> blend_const = {
                        static_cast<u8>(output_merger.blend_const.r),
                        static_cast<u8>(output_merger.blend_const.g),
                        static_cast<u8>(output_merger.blend_const.b),
                        static_cast<u8>(output_merger.blend_const.a)
                    };

                    switch (factor) {
                    case Regs::BlendFactor::Zero:
                        return 0;

                    case Regs::BlendFactor::One:
                        return 255;

                    case Regs::BlendFactor::SourceColor:
                        return combiner_output[channel];

                    case Regs::BlendFactor::OneMinusSourceColor:
                        return 255 - combiner_output[channel];

                    case Regs::BlendFactor::DestColor:
                        return dest[channel];

                    case Regs::BlendFactor::OneMinusDestColor:
                        return 255 - dest[channel];

                    case Regs::BlendFactor::SourceAlpha:
                        return combiner_output.a();

                    case Regs::BlendFactor::OneMinusSourceAlpha:
                        return 255 - combiner_output.a();

                    case Regs::BlendFactor::DestAlpha:
                        return dest.a();

                    case Regs::BlendFactor::OneMinusDestAlpha:
                        return 255 - dest.a();

                    case Regs::BlendFactor::ConstantColor:
                        return blend_const[channel];

                    case Regs::BlendFactor::OneMinusConstantColor:
                        return 255 - blend_const[channel];

                    case Regs::BlendFactor::ConstantAlpha:
                        return blend_const.a();

                    case Regs::BlendFactor::OneMinusConstantAlpha:
                        return 255 - blend_const.a();

                    case Regs::BlendFactor::SourceAlphaSaturate:
                        // Returns 1.0 for the alpha channel
                        if (channel == 3)
                            return 255;
                        return std::min(combiner_output.a(), static_cast<u8>(255 - dest.a()));

                    default:
                        LOG_CRITICAL(HW_GPU, "Unknown blend factor %x", factor);
                        UNIMPLEMENTED();
                        break;
                    }

                    return combiner_output[channel];
                };

                static auto EvaluateBlendEquation = [](const Math::Vec4<u8>& src, const Math::Vec4<u8>& srcfactor,
                                                       const Math::Vec4<u8>& dest, const Math::Vec4<u8>& destfactor,
                                                       Regs::BlendEquation equation) {
                    Math::Vec4<int> result;

                    auto src_result = (src  *  srcfactor).Cast<int>();
                    auto dst_result = (dest * destfactor).Cast<int>();

                    switch (equation) {
                    case Regs::BlendEquation::Add:
                        result = (src_result + dst_result) / 255;
                        break;

                    case Regs::BlendEquation::Subtract:
                        result = (src_result - dst_result) / 255;
                        break;

                    case Regs::BlendEquation::ReverseSubtract:
                        result = (dst_result - src_result) / 255;
                        break;

                    // TODO: How do these two actually work?
                    //       OpenGL doesn't include the blend factors in the min/max computations,
                    //       but is this what the 3DS actually does?
                    case Regs::BlendEquation::Min:
                        result.r() = std::min(src.r(), dest.r());
                        result.g() = std::min(src.g(), dest.g());
                        result.b() = std::min(src.b(), dest.b());
                        result.a() = std::min(src.a(), dest.a());
                        break;

                    case Regs::BlendEquation::Max:
                        result.r() = std::max(src.r(), dest.r());
                        result.g() = std::max(src.g(), dest.g());
                        result.b() = std::max(src.b(), dest.b());
                        result.a() = std::max(src.a(), dest.a());
                        break;

                    default:
                        LOG_CRITICAL(HW_GPU, "Unknown RGB blend equation %x", equation);
                        UNIMPLEMENTED();
                    }

                    return Math::Vec4<u8>(MathUtil::Clamp(result.r(), 0, 255),
                                    MathUtil::Clamp(result.g(), 0, 255),
                                    MathUtil::Clamp(result.b(), 0, 255),
                                    MathUtil::Clamp(result.a(), 0, 255));
                };

                auto srcfactor = Math::MakeVec(LookupFactor(0, params.factor_source_rgb),
                                               LookupFactor(1, params.factor_source_rgb),
                                               LookupFactor(2, params.factor_source_rgb),
                                               LookupFactor(3, params.factor_source_a));

                auto dstfactor = Math::MakeVec(LookupFactor(0, params.factor_dest_rgb),
                                               LookupFactor(1, params.factor_dest_rgb),
                                               LookupFactor(2, params.factor_dest_rgb),
                                               LookupFactor(3, params.factor_dest_a));

                blend_output     = EvaluateBlendEquation(combiner_output, srcfactor, dest, dstfactor, params.blend_equation_rgb);
                blend_output.a() = EvaluateBlendEquation(combiner_output, srcfactor, dest, dstfactor, params.blend_equation_a).a();
            } else {
                static auto LogicOp = [](u8 src, u8 dest, Regs::LogicOp op) -> u8 {
                    switch (op) {
                    case Regs::LogicOp::Clear:
                        return 0;

                    case Regs::LogicOp::And:
                        return src & dest;

                    case Regs::LogicOp::AndReverse:
                        return src & ~dest;

                    case Regs::LogicOp::Copy:
                        return src;

                    case Regs::LogicOp::Set:
                        return 255;

                    case Regs::LogicOp::CopyInverted:
                        return ~src;

                    case Regs::LogicOp::NoOp:
                        return dest;

                    case Regs::LogicOp::Invert:
                        return ~dest;

                    case Regs::LogicOp::Nand:
                        return ~(src & dest);

                    case Regs::LogicOp::Or:
                        return src | dest;

                    case Regs::LogicOp::Nor:
                        return ~(src | dest);

                    case Regs::LogicOp::Xor:
                        return src ^ dest;

                    case Regs::LogicOp::Equiv:
                        return ~(src ^ dest);

                    case Regs::LogicOp::AndInverted:
                        return ~src & dest;

                    case Regs::LogicOp::OrReverse:
                        return src | ~dest;

                    case Regs::LogicOp::OrInverted:
                        return ~src | dest;
                    }
                };

                blend_output = Math::MakeVec(
                    LogicOp(combiner_output.r(), dest.r(), output_merger.logic_op),
                    LogicOp(combiner_output.g(), dest.g(), output_merger.logic_op),
                    LogicOp(combiner_output.b(), dest.b(), output_merger.logic_op),
                    LogicOp(combiner_output.a(), dest.a(), output_merger.logic_op));
            }

            const Math::Vec4<u8> result = {
                output_merger.red_enable   ? blend_output.r() : dest.r(),
                output_merger.green_enable ? blend_output.g() : dest.g(),
                output_merger.blue_enable  ? blend_output.b() : dest.b(),
                output_merger.alpha_enable ? blend_output.a() : dest.a()
            };

            if (regs.framebuffer.allow_color_write != 0)
                DrawPixel(x >> 4, y >> 4, result);
        }
    }
}

void ProcessTriangle(const Shader::OutputVertex& v0,
                     const Shader::OutputVertex& v1,
                     const Shader::OutputVertex& v2) {
    ProcessTriangleInternal(v0, v1, v2);
}

} // namespace Rasterizer

} // namespace Pica
