// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

//? #version 450

out gl_PerVertex {
    vec4 gl_Position;
};

layout(location = 0) out vec2 texcoord;

#ifdef VULKAN
#define gl_VertexID gl_VertexIndex
#define BEGIN_PUSH_CONSTANTS layout(push_constant) uniform PushConstants {
#define END_PUSH_CONSTANTS };
#define UNIFORM(n)
#else // if OpenGL
#define BEGIN_PUSH_CONSTANTS
#define END_PUSH_CONSTANTS
#define UNIFORM(n) layout (location = n) uniform
#endif

BEGIN_PUSH_CONSTANTS
UNIFORM(0) vec2 tex_scale;
UNIFORM(1) vec2 tex_offset;
END_PUSH_CONSTANTS

void main() {
    float x = float((gl_VertexID & 1) << 2);
    float y = float((gl_VertexID & 2) << 1);
    gl_Position = vec4(x - 1.0, y - 1.0, 0.0, 1.0);
    texcoord = fma(vec2(x, y) / 2.0, tex_scale, tex_offset);
}
