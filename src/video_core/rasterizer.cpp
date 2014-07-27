// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>

#include "common/common_types.h"

#include "math.h"
#include "pica.h"
#include "rasterizer.h"
#include "vertex_shader.h"

namespace Pica {

namespace Rasterizer {

static void DrawPixel(int x, int y, const Math::Vec4<u8>& color) {
    u32* color_buffer = (u32*)Memory::GetPointer(registers.framebuffer.GetColorBufferAddress());
    u32 value = (color.a() << 24) | (color.r() << 16) | (color.g() << 8) | color.b();

    // Assuming RGBA8 format until actual framebuffer format handling is implemented
    *(color_buffer + x + y * registers.framebuffer.GetWidth() / 2) = value;
}

static u32 GetDepth(int x, int y) {
    u16* depth_buffer = (u16*)Memory::GetPointer(registers.framebuffer.GetDepthBufferAddress());

    // Assuming 16-bit depth buffer format until actual format handling is implemented
    return *(depth_buffer + x + y * registers.framebuffer.GetWidth() / 2);
}

static void SetDepth(int x, int y, u16 value) {
    u16* depth_buffer = (u16*)Memory::GetPointer(registers.framebuffer.GetDepthBufferAddress());

    // Assuming 16-bit depth buffer format until actual format handling is implemented
    *(depth_buffer + x + y * registers.framebuffer.GetWidth() / 2) = value;
}

void ProcessTriangle(const VertexShader::OutputVertex& v0,
                     const VertexShader::OutputVertex& v1,
                     const VertexShader::OutputVertex& v2)
{
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

    // vertex positions in rasterizer coordinates
    auto FloatToFix = [](float24 flt) {
                          return Fix12P4(flt.ToFloat32() * 16.0f);
                      };
    auto ScreenToRasterizerCoordinates = [FloatToFix](const Math::Vec3<float24> vec) {
                                             return Math::Vec3<Fix12P4>{FloatToFix(vec.x), FloatToFix(vec.y), FloatToFix(vec.z)};
                                         };
    Math::Vec3<Fix12P4> vtxpos[3]{ ScreenToRasterizerCoordinates(v0.screenpos),
                                   ScreenToRasterizerCoordinates(v1.screenpos),
                                   ScreenToRasterizerCoordinates(v2.screenpos) };

    // TODO: Proper scissor rect test!
    u16 min_x = std::min({vtxpos[0].x, vtxpos[1].x, vtxpos[2].x});
    u16 min_y = std::min({vtxpos[0].y, vtxpos[1].y, vtxpos[2].y});
    u16 max_x = std::max({vtxpos[0].x, vtxpos[1].x, vtxpos[2].x});
    u16 max_y = std::max({vtxpos[0].y, vtxpos[1].y, vtxpos[2].y});

    min_x = min_x & Fix12P4::IntMask();
    min_y = min_y & Fix12P4::IntMask();
    max_x = (max_x + Fix12P4::FracMask()) & Fix12P4::IntMask();
    max_y = (max_y + Fix12P4::FracMask()) & Fix12P4::IntMask();

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

    // TODO: Not sure if looping through x first might be faster
    for (u16 y = min_y; y < max_y; y += 0x10) {
        for (u16 x = min_x; x < max_x; x += 0x10) {

            // Calculate the barycentric coordinates w0, w1 and w2
            auto orient2d = [](const Math::Vec2<Fix12P4>& vtx1,
                               const Math::Vec2<Fix12P4>& vtx2,
                               const Math::Vec2<Fix12P4>& vtx3) {
                const auto vec1 = (vtx2.Cast<int>() - vtx1.Cast<int>()).Append(0);
                const auto vec2 = (vtx3.Cast<int>() - vtx1.Cast<int>()).Append(0);
                // TODO: There is a very small chance this will overflow for sizeof(int) == 4
                return Cross(vec1, vec2).z;
            };

            int w0 = bias0 + orient2d(vtxpos[1].xy(), vtxpos[2].xy(), {x, y});
            int w1 = bias1 + orient2d(vtxpos[2].xy(), vtxpos[0].xy(), {x, y});
            int w2 = bias2 + orient2d(vtxpos[0].xy(), vtxpos[1].xy(), {x, y});
            int wsum = w0 + w1 + w2;

            // If current pixel is not covered by the current primitive
            if (w0 < 0 || w1 < 0 || w2 < 0)
                continue;

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
                auto attr_over_w = Math::MakeVec3(attr0 / v0.pos.w,
                                                  attr1 / v1.pos.w,
                                                  attr2 / v2.pos.w);
                auto w_inverse   = Math::MakeVec3(float24::FromFloat32(1.f) / v0.pos.w,
                                                  float24::FromFloat32(1.f) / v1.pos.w,
                                                  float24::FromFloat32(1.f) / v2.pos.w);
                auto baricentric_coordinates = Math::MakeVec3(float24::FromFloat32(w0),
                                                              float24::FromFloat32(w1),
                                                              float24::FromFloat32(w2));

                float24 interpolated_attr_over_w = Math::Dot(attr_over_w, baricentric_coordinates);
                float24 interpolated_w_inverse   = Math::Dot(w_inverse,   baricentric_coordinates);
                return interpolated_attr_over_w / interpolated_w_inverse;
            };

            Math::Vec4<u8> primary_color{
                (u8)(GetInterpolatedAttribute(v0.color.r(), v1.color.r(), v2.color.r()).ToFloat32() * 255),
                (u8)(GetInterpolatedAttribute(v0.color.g(), v1.color.g(), v2.color.g()).ToFloat32() * 255),
                (u8)(GetInterpolatedAttribute(v0.color.b(), v1.color.b(), v2.color.b()).ToFloat32() * 255),
                (u8)(GetInterpolatedAttribute(v0.color.a(), v1.color.a(), v2.color.a()).ToFloat32() * 255)
            };

            u16 z = (u16)(((float)v0.screenpos[2].ToFloat32() * w0 +
                           (float)v1.screenpos[2].ToFloat32() * w1 +
                           (float)v2.screenpos[2].ToFloat32() * w2) * 65535.f / wsum); // TODO: Shouldn't need to multiply by 65536?
            SetDepth(x >> 4, y >> 4, z);

            DrawPixel(x >> 4, y >> 4, primary_color);
        }
    }
}

} // namespace Rasterizer

} // namespace Pica
