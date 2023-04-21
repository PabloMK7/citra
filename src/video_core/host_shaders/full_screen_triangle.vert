// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

//? #version 450

out gl_PerVertex {
    vec4 gl_Position;
};

layout(location = 0) out vec2 texcoord;

layout (location = 0) uniform vec2 tex_scale;
layout (location = 1) uniform vec2 tex_offset;

void main() {
    float x = float((gl_VertexID & 1) << 2);
    float y = float((gl_VertexID & 2) << 1);
    gl_Position = vec4(x - 1.0, y - 1.0, 0.0, 1.0);
    texcoord = fma(vec2(x, y) / 2.0, tex_scale, tex_offset);
}
