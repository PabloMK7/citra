// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

//? #version 430 core

layout(location = 0) in mediump vec2 dst_coord;
layout(location = 0) out lowp vec4 frag_color;

layout(binding = 0) uniform highp sampler2D depth;
layout(binding = 1) uniform lowp usampler2D stencil;
uniform mediump ivec2 dst_size;
uniform mediump ivec2 src_size;
uniform mediump ivec2 src_offset;

void main() {
    mediump ivec2 tex_coord;
    if (src_size == dst_size) {
        tex_coord = ivec2(dst_coord);
    } else {
        highp int tex_index = int(dst_coord.y) * dst_size.x + int(dst_coord.x);
        mediump int y = tex_index / src_size.x;
        tex_coord = ivec2(tex_index - y * src_size.x, y);
    }
    tex_coord -= src_offset;

    highp uint depth_val =
        uint(texelFetch(depth, tex_coord, 0).x * (exp2(32.0) - 1.0));
    lowp uint stencil_val = texelFetch(stencil, tex_coord, 0).x;
    highp uvec4 components =
        uvec4(stencil_val, (uvec3(depth_val) >> uvec3(24u, 16u, 8u)) & 0x000000FFu);
    frag_color = vec4(components) / (exp2(8.0) - 1.0);
}
