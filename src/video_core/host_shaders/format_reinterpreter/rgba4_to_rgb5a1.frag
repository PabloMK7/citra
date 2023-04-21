// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

//? #version 430 core

layout(location = 0) in mediump vec2 dst_coord;
layout(location = 0) out lowp vec4 frag_color;

layout(binding = 0) uniform lowp sampler2D source;
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

    lowp ivec4 rgba4 = ivec4(texelFetch(source, tex_coord, 0) * (exp2(4.0) - 1.0));
    lowp ivec3 rgb5 =
        ((rgba4.rgb << ivec3(1, 2, 3)) | (rgba4.gba >> ivec3(3, 2, 1))) & 0x1F;
    frag_color = vec4(vec3(rgb5) / (exp2(5.0) - 1.0), rgba4.a & 0x01);
}
