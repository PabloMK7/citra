// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>

#include "common/common_types.h"
#include "common/math_util.h"

#include "core/hw/gpu.h"
#include "debug_utils/debug_utils.h"
#include "math.h"
#include "color.h"
#include "pica.h"
#include "rasterizer.h"
#include "vertex_shader.h"
#include "video_core/utils.h"

namespace Pica {

namespace Rasterizer {

static void DrawPixel(int x, int y, const Math::Vec4<u8>& color) {
    const PAddr addr = registers.framebuffer.GetColorBufferPhysicalAddress();
    u8* color_buffer = Memory::GetPointer(PAddrToVAddr(addr));

    // Similarly to textures, the render framebuffer is laid out from bottom to top, too.
    // NOTE: The framebuffer height register contains the actual FB height minus one.
    y = (registers.framebuffer.height - y);

    const u32 coarse_y = y & ~7;
    u32 bytes_per_pixel = GPU::Regs::BytesPerPixel(GPU::Regs::PixelFormat(registers.framebuffer.color_format.Value()));
    u32 dst_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * registers.framebuffer.width * bytes_per_pixel;

    switch (registers.framebuffer.color_format) {
    case registers.framebuffer.RGBA8:
    {
        u8* pixel = color_buffer + dst_offset;
        pixel[3] = color.r();
        pixel[2] = color.g();
        pixel[1] = color.b();
        pixel[0] = color.a();
        break;
    }

    case registers.framebuffer.RGBA4:
    {
        u8* pixel = color_buffer + dst_offset;
        pixel[1] = (color.r() & 0xF0) | (color.g() >> 4);
        pixel[0] = (color.b() & 0xF0) | (color.a() >> 4);
        break;
    }

    default:
        LOG_CRITICAL(Render_Software, "Unknown framebuffer color format %x", registers.framebuffer.color_format.Value());
        UNIMPLEMENTED();
    }
}

static const Math::Vec4<u8> GetPixel(int x, int y) {
    const PAddr addr = registers.framebuffer.GetColorBufferPhysicalAddress();
    u8* color_buffer = Memory::GetPointer(PAddrToVAddr(addr));

    y = (registers.framebuffer.height - y);

    const u32 coarse_y = y & ~7;
    u32 bytes_per_pixel = GPU::Regs::BytesPerPixel(GPU::Regs::PixelFormat(registers.framebuffer.color_format.Value()));
    u32 src_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * registers.framebuffer.width * bytes_per_pixel;

    switch (registers.framebuffer.color_format) {
    case registers.framebuffer.RGBA8:
    {
        Math::Vec4<u8> ret;
        u8* pixel = color_buffer + src_offset;
        ret.r() = pixel[3];
        ret.g() = pixel[2];
        ret.b() = pixel[1];
        ret.a() = pixel[0];
        return ret;
    }

    case registers.framebuffer.RGBA4:
    {
        Math::Vec4<u8> ret;
        u8* pixel = color_buffer + src_offset;
        ret.r() = Color::Convert4To8(pixel[1] >> 4);
        ret.g() = Color::Convert4To8(pixel[1] & 0x0F);
        ret.b() = Color::Convert4To8(pixel[0] >> 4);
        ret.a() = Color::Convert4To8(pixel[0] & 0x0F);
        return ret;
    }

    default:
        LOG_CRITICAL(Render_Software, "Unknown framebuffer color format %x", registers.framebuffer.color_format.Value());
        UNIMPLEMENTED();
    }

    return {};
 }

static u32 GetDepth(int x, int y) {
    const PAddr addr = registers.framebuffer.GetDepthBufferPhysicalAddress();
    u8* depth_buffer = Memory::GetPointer(PAddrToVAddr(addr));

    y = (registers.framebuffer.height - y);
    
    const u32 coarse_y = y & ~7;
    u32 stride = registers.framebuffer.width * 2;

    // Assuming 16-bit depth buffer format until actual format handling is implemented
    return *(u16*)(depth_buffer + VideoCore::GetMortonOffset(x, y, 2) + coarse_y * stride);
}

static void SetDepth(int x, int y, u16 value) {
    const PAddr addr = registers.framebuffer.GetDepthBufferPhysicalAddress();
    u8* depth_buffer = Memory::GetPointer(PAddrToVAddr(addr));

    y = (registers.framebuffer.height - y);

    const u32 coarse_y = y & ~7;
    u32 stride = registers.framebuffer.width * 2;

    // Assuming 16-bit depth buffer format until actual format handling is implemented
    *(u16*)(depth_buffer + VideoCore::GetMortonOffset(x, y, 2) + coarse_y * stride) = value;
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

/**
 * Helper function for ProcessTriangle with the "reversed" flag to allow for implementing
 * culling via recursion.
 */
static void ProcessTriangleInternal(const VertexShader::OutputVertex& v0,
                                    const VertexShader::OutputVertex& v1,
                                    const VertexShader::OutputVertex& v2,
                                    bool reversed = false)
{
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

    if (registers.cull_mode == Regs::CullMode::KeepAll) {
        // Make sure we always end up with a triangle wound counter-clockwise
        if (!reversed && SignedArea(vtxpos[0].xy(), vtxpos[1].xy(), vtxpos[2].xy()) <= 0) {
            ProcessTriangleInternal(v0, v2, v1, true);
            return;
        }
    } else {
        if (!reversed && registers.cull_mode == Regs::CullMode::KeepClockWise) {
            // Reverse vertex order and use the CCW code path.
            ProcessTriangleInternal(v0, v2, v1, true);
            return;
        }

        // Cull away triangles which are wound clockwise.
        if (SignedArea(vtxpos[0].xy(), vtxpos[1].xy(), vtxpos[2].xy()) <= 0)
            return;
    }

    // TODO: Proper scissor rect test!
    u16 min_x = std::min({vtxpos[0].x, vtxpos[1].x, vtxpos[2].x});
    u16 min_y = std::min({vtxpos[0].y, vtxpos[1].y, vtxpos[2].y});
    u16 max_x = std::max({vtxpos[0].x, vtxpos[1].x, vtxpos[2].x});
    u16 max_y = std::max({vtxpos[0].y, vtxpos[1].y, vtxpos[2].y});

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

    auto textures = registers.GetTextures();
    auto tev_stages = registers.GetTevStages();

    // Enter rasterization loop, starting at the center of the topleft bounding box corner.
    // TODO: Not sure if looping through x first might be faster
    for (u16 y = min_y + 8; y < max_y; y += 0x10) {
        for (u16 x = min_x + 8; x < max_x; x += 0x10) {

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

                int s = (int)(uv[i].u() * float24::FromFloat32(static_cast<float>(texture.config.width))).ToFloat32();
                int t = (int)(uv[i].v() * float24::FromFloat32(static_cast<float>(texture.config.height))).ToFloat32();
                static auto GetWrappedTexCoord = [](Regs::TextureConfig::WrapMode mode, int val, unsigned size) {
                    switch (mode) {
                        case Regs::TextureConfig::ClampToEdge:
                            val = std::max(val, 0);
                            val = std::min(val, (int)size - 1);
                            return val;

                        case Regs::TextureConfig::Repeat:
                            return (int)((unsigned)val % size);

                        case Regs::TextureConfig::MirroredRepeat:
                        {
                            int coord = (int)((unsigned)val % (2 * size));
                            if (coord >= size)
                                coord = 2 * size - 1 - coord;
                            return coord;
                        }

                        default:
                            LOG_ERROR(HW_GPU, "Unknown texture coordinate wrapping mode %x\n", (int)mode);
                            UNIMPLEMENTED();
                            return 0;
                    }
                };

                // Textures are laid out from bottom to top, hence we invert the t coordinate.
                // NOTE: This may not be the right place for the inversion.
                // TODO: Check if this applies to ETC textures, too.
                s = GetWrappedTexCoord(texture.config.wrap_s, s, texture.config.width);
                t = texture.config.height - 1 - GetWrappedTexCoord(texture.config.wrap_t, t, texture.config.height);

                u8* texture_data = Memory::GetPointer(PAddrToVAddr(texture.config.GetPhysicalAddress()));
                auto info = DebugUtils::TextureInfo::FromPicaRegister(texture.config, texture.format);

                texture_color[i] = DebugUtils::LookupTexture(texture_data, s, t, info);
                DebugUtils::DumpTexture(texture.config, texture_data);
            }

            // Texture environment - consists of 6 stages of color and alpha combining.
            //
            // Color combiners take three input color values from some source (e.g. interpolated
            // vertex color, texture color, previous stage, etc), perform some very simple
            // operations on each of them (e.g. inversion) and then calculate the output color
            // with some basic arithmetic. Alpha combiners can be configured separately but work
            // analogously.
            Math::Vec4<u8> combiner_output;
            for (const auto& tev_stage : tev_stages) {
                using Source = Regs::TevStageConfig::Source;
                using ColorModifier = Regs::TevStageConfig::ColorModifier;
                using AlphaModifier = Regs::TevStageConfig::AlphaModifier;
                using Operation = Regs::TevStageConfig::Operation;

                auto GetSource = [&](Source source) -> Math::Vec4<u8> {
                    switch (source) {
                    // TODO: What's the difference between these two?
                    case Source::PrimaryColor:
                    case Source::PrimaryFragmentColor:
                        return primary_color;

                    case Source::Texture0:
                        return texture_color[0];

                    case Source::Texture1:
                        return texture_color[1];

                    case Source::Texture2:
                        return texture_color[2];

                    case Source::Constant:
                        return {tev_stage.const_r, tev_stage.const_g, tev_stage.const_b, tev_stage.const_a};

                    case Source::Previous:
                        return combiner_output;

                    default:
                        LOG_ERROR(HW_GPU, "Unknown color combiner source %d\n", (int)source);
                        UNIMPLEMENTED();
                        return {};
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

                    default:
                        LOG_ERROR(HW_GPU, "Unknown color combiner operation %d\n", (int)op);
                        UNIMPLEMENTED();
                        return {};
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

                    case Operation::Lerp:
                        return (input[0] * input[2] + input[1] * (255 - input[2])) / 255;

                    case Operation::Subtract:
                        return std::max(0, (int)input[0] - (int)input[1]);

                    case Operation::MultiplyThenAdd:
                        return std::min(255, (input[0] * input[1] + 255 * input[2]) / 255);

                    case Operation::AddThenMultiply:
                        return (std::min(255, (input[0] + input[1])) * input[2]) / 255;

                    default:
                        LOG_ERROR(HW_GPU, "Unknown alpha combiner operation %d\n", (int)op);
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
                std::array<u8,3> alpha_result = {
                    GetAlphaModifier(tev_stage.alpha_modifier1, GetSource(tev_stage.alpha_source1)),
                    GetAlphaModifier(tev_stage.alpha_modifier2, GetSource(tev_stage.alpha_source2)),
                    GetAlphaModifier(tev_stage.alpha_modifier3, GetSource(tev_stage.alpha_source3))
                };
                auto alpha_output = AlphaCombine(tev_stage.alpha_op, alpha_result);

                combiner_output = Math::MakeVec(color_output, alpha_output);
            }

            if (registers.output_merger.alpha_test.enable) {
                bool pass = false;

                switch (registers.output_merger.alpha_test.func) {
                case registers.output_merger.Never:
                    pass = false;
                    break;

                case registers.output_merger.Always:
                    pass = true;
                    break;

                case registers.output_merger.Equal:
                    pass = combiner_output.a() == registers.output_merger.alpha_test.ref;
                    break;

                case registers.output_merger.NotEqual:
                    pass = combiner_output.a() != registers.output_merger.alpha_test.ref;
                    break;

                case registers.output_merger.LessThan:
                    pass = combiner_output.a() < registers.output_merger.alpha_test.ref;
                    break;

                case registers.output_merger.LessThanOrEqual:
                    pass = combiner_output.a() <= registers.output_merger.alpha_test.ref;
                    break;

                case registers.output_merger.GreaterThan:
                    pass = combiner_output.a() > registers.output_merger.alpha_test.ref;
                    break;

                case registers.output_merger.GreaterThanOrEqual:
                    pass = combiner_output.a() >= registers.output_merger.alpha_test.ref;
                    break;
                }

                if (!pass)
                    continue;
            }

            // TODO: Does depth indeed only get written even if depth testing is enabled?
            if (registers.output_merger.depth_test_enable) {
                u16 z = (u16)((v0.screenpos[2].ToFloat32() * w0 +
                            v1.screenpos[2].ToFloat32() * w1 +
                            v2.screenpos[2].ToFloat32() * w2) * 65535.f / wsum);
                u16 ref_z = GetDepth(x >> 4, y >> 4);

                bool pass = false;

                switch (registers.output_merger.depth_test_func) {
                case registers.output_merger.Never:
                    pass = false;
                    break;

                case registers.output_merger.Always:
                    pass = true;
                    break;

                case registers.output_merger.Equal:
                    pass = z == ref_z;
                    break;

                case registers.output_merger.NotEqual:
                    pass = z != ref_z;
                    break;

                case registers.output_merger.LessThan:
                    pass = z < ref_z;
                    break;

                case registers.output_merger.LessThanOrEqual:
                    pass = z <= ref_z;
                    break;

                case registers.output_merger.GreaterThan:
                    pass = z > ref_z;
                    break;

                case registers.output_merger.GreaterThanOrEqual:
                    pass = z >= ref_z;
                    break;
                }

                if (!pass)
                    continue;

                if (registers.output_merger.depth_write_enable)
                    SetDepth(x >> 4, y >> 4, z);
            }

            auto dest = GetPixel(x >> 4, y >> 4);
            Math::Vec4<u8> blend_output = combiner_output;

            if (registers.output_merger.alphablend_enable) {
                auto params = registers.output_merger.alpha_blending;

                auto LookupFactorRGB = [&](decltype(params)::BlendFactor factor) -> Math::Vec3<u8> {
                    switch (factor) {
                    case params.Zero:
                        return Math::Vec3<u8>(0, 0, 0);

                    case params.One:
                        return Math::Vec3<u8>(255, 255, 255);

                    case params.SourceColor:
                        return combiner_output.rgb();

                    case params.OneMinusSourceColor:
                        return Math::Vec3<u8>(255 - combiner_output.r(), 255 - combiner_output.g(), 255 - combiner_output.b());

                    case params.DestColor:
                        return dest.rgb();

                    case params.OneMinusDestColor:
                        return Math::Vec3<u8>(255 - dest.r(), 255 - dest.g(), 255 - dest.b());

                    case params.SourceAlpha:
                        return Math::Vec3<u8>(combiner_output.a(), combiner_output.a(), combiner_output.a());

                    case params.OneMinusSourceAlpha:
                        return Math::Vec3<u8>(255 - combiner_output.a(), 255 - combiner_output.a(), 255 - combiner_output.a());

                    case params.DestAlpha:
                        return Math::Vec3<u8>(dest.a(), dest.a(), dest.a());

                    case params.OneMinusDestAlpha:
                        return Math::Vec3<u8>(255 - dest.a(), 255 - dest.a(), 255 - dest.a());

                    case params.ConstantColor:
                        return Math::Vec3<u8>(registers.output_merger.blend_const.r, registers.output_merger.blend_const.g, registers.output_merger.blend_const.b);

                    case params.OneMinusConstantColor:
                        return Math::Vec3<u8>(255 - registers.output_merger.blend_const.r, 255 - registers.output_merger.blend_const.g, 255 - registers.output_merger.blend_const.b);

                    case params.ConstantAlpha:
                        return Math::Vec3<u8>(registers.output_merger.blend_const.a, registers.output_merger.blend_const.a, registers.output_merger.blend_const.a);

                    case params.OneMinusConstantAlpha:
                        return Math::Vec3<u8>(255 - registers.output_merger.blend_const.a, 255 - registers.output_merger.blend_const.a, 255 - registers.output_merger.blend_const.a);

                    default:
                        LOG_CRITICAL(HW_GPU, "Unknown color blend factor %x", factor);
                        UNIMPLEMENTED();
                        break;
                    }
                };

                auto LookupFactorA = [&](decltype(params)::BlendFactor factor) -> u8 {
                    switch (factor) {
                    case params.Zero:
                        return 0;

                    case params.One:
                        return 255;

                    case params.SourceAlpha:
                        return combiner_output.a();

                    case params.OneMinusSourceAlpha:
                        return 255 - combiner_output.a();

                    case params.DestAlpha:
                        return dest.a();

                    case params.OneMinusDestAlpha:
                        return 255 - dest.a();

                    case params.ConstantAlpha:
                        return registers.output_merger.blend_const.a;

                    case params.OneMinusConstantAlpha:
                        return 255 - registers.output_merger.blend_const.a;

                    default:
                        LOG_CRITICAL(HW_GPU, "Unknown alpha blend factor %x", factor);
                        UNIMPLEMENTED();
                        break;
                    }
                };

                using BlendEquation = decltype(params)::BlendEquation;
                static auto EvaluateBlendEquation = [](const Math::Vec4<u8>& src, const Math::Vec4<u8>& srcfactor,
                                                       const Math::Vec4<u8>& dest, const Math::Vec4<u8>& destfactor,
                                                       BlendEquation equation) {
                    Math::Vec4<int> result;

                    auto src_result = (src  *  srcfactor).Cast<int>();
                    auto dst_result = (dest * destfactor).Cast<int>();

                    switch (equation) {
                    case BlendEquation::Add:
                        result = (src_result + dst_result) / 255;
                        break;

                    case BlendEquation::Subtract:
                        result = (src_result - dst_result) / 255;
                        break;

                    case BlendEquation::ReverseSubtract:
                        result = (dst_result - src_result) / 255;
                        break;

                    // TODO: How do these two actually work?
                    //       OpenGL doesn't include the blend factors in the min/max computations,
                    //       but is this what the 3DS actually does?
                    case BlendEquation::Min:
                        result.r() = std::min(src.r(), dest.r());
                        result.g() = std::min(src.g(), dest.g());
                        result.b() = std::min(src.b(), dest.b());
                        result.a() = std::min(src.a(), dest.a());
                        break;

                    case BlendEquation::Max:
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

                auto srcfactor = Math::MakeVec(LookupFactorRGB(params.factor_source_rgb),
                                               LookupFactorA(params.factor_source_a));
                auto dstfactor = Math::MakeVec(LookupFactorRGB(params.factor_dest_rgb),
                                               LookupFactorA(params.factor_dest_a));

                blend_output     = EvaluateBlendEquation(combiner_output, srcfactor, dest, dstfactor, params.blend_equation_rgb);
                blend_output.a() = EvaluateBlendEquation(combiner_output, srcfactor, dest, dstfactor, params.blend_equation_a).a();
            } else {
                LOG_CRITICAL(HW_GPU, "logic op: %x", registers.output_merger.logic_op);
                UNIMPLEMENTED();
            }

            const Math::Vec4<u8> result = {
                registers.output_merger.red_enable   ? blend_output.r() : dest.r(),
                registers.output_merger.green_enable ? blend_output.g() : dest.g(),
                registers.output_merger.blue_enable  ? blend_output.b() : dest.b(),
                registers.output_merger.alpha_enable ? blend_output.a() : dest.a()
            };

            DrawPixel(x >> 4, y >> 4, result);
        }
    }
}

void ProcessTriangle(const VertexShader::OutputVertex& v0,
                     const VertexShader::OutputVertex& v1,
                     const VertexShader::OutputVertex& v2) {
    ProcessTriangleInternal(v0, v1, v2);
}

} // namespace Rasterizer

} // namespace Pica
