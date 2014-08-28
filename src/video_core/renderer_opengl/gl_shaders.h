// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

namespace GLShaders {

static const char g_vertex_shader[] = R"(
#version 150 core
in vec3 position;
in vec2 texCoord;

out vec2 UV;

mat3 window_scale = mat3(
                         vec3(1.0, 0.0, 0.0),
                         vec3(0.0, 5.0/6.0, 0.0), // TODO(princesspeachum): replace hard-coded aspect with uniform
                         vec3(0.0, 0.0, 1.0)
                         );

void main() {
    gl_Position.xyz = window_scale * position;
    gl_Position.w = 1.0;

    UV = texCoord;
})";

static const char g_fragment_shader[] = R"(
#version 150 core
in vec2 UV;
out vec3 color;
uniform sampler2D sampler;

void main() {
    color = texture(sampler, UV).rgb;
})";

}
