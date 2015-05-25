// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace GLShaders {

const char g_vertex_shader[] = R"(
#version 150 core

in vec2 vert_position;
in vec2 vert_tex_coord;
out vec2 frag_tex_coord;

// This is a truncated 3x3 matrix for 2D transformations:
// The upper-left 2x2 submatrix performs scaling/rotation/mirroring.
// The third column performs translation.
// The third row could be used for projection, which we don't need in 2D. It hence is assumed to
// implicitly be [0, 0, 1]
uniform mat3x2 modelview_matrix;

void main() {
    // Multiply input position by the rotscale part of the matrix and then manually translate by
    // the last column. This is equivalent to using a full 3x3 matrix and expanding the vector
    // to `vec3(vert_position.xy, 1.0)`
    gl_Position = vec4(mat2(modelview_matrix) * vert_position + modelview_matrix[2], 0.0, 1.0);
    frag_tex_coord = vert_tex_coord;
}
)";

const char g_fragment_shader[] = R"(
#version 150 core

in vec2 frag_tex_coord;
out vec4 color;

uniform sampler2D color_texture;

void main() {
    color = texture(color_texture, frag_tex_coord);
}
)";

const char g_vertex_shader_hw[] = R"(
#version 150 core

#define NUM_VTX_ATTR 7

in vec4 vert_position;
in vec4 vert_color;
in vec2 vert_texcoords[3];

out vec4 o[NUM_VTX_ATTR];

void main() {
    o[2] = vert_color;
    o[3] = vec4(vert_texcoords[0].xy, vert_texcoords[1].xy);
    o[5] = vec4(0.0, 0.0, vert_texcoords[2].xy);

    gl_Position = vec4(vert_position.x, -vert_position.y, -vert_position.z, vert_position.w);
}
)";

// TODO: Create a shader constructor and cache that builds this program with minimal conditionals instead of using tev_cfg uniforms
const char g_fragment_shader_hw[] = R"(
#version 150 core

#define NUM_VTX_ATTR 7
#define NUM_TEV_STAGES 6

#define SOURCE_PRIMARYCOLOR           0x0
#define SOURCE_PRIMARYFRAGMENTCOLOR   0x1
#define SOURCE_SECONDARYFRAGMENTCOLOR 0x2
#define SOURCE_TEXTURE0               0x3
#define SOURCE_TEXTURE1               0x4
#define SOURCE_TEXTURE2               0x5
#define SOURCE_TEXTURE3               0x6
#define SOURCE_PREVIOUSBUFFER         0xd
#define SOURCE_CONSTANT               0xe
#define SOURCE_PREVIOUS               0xf

#define COLORMODIFIER_SOURCECOLOR         0x0
#define COLORMODIFIER_ONEMINUSSOURCECOLOR 0x1
#define COLORMODIFIER_SOURCEALPHA         0x2
#define COLORMODIFIER_ONEMINUSSOURCEALPHA 0x3
#define COLORMODIFIER_SOURCERED           0x4
#define COLORMODIFIER_ONEMINUSSOURCERED   0x5
#define COLORMODIFIER_SOURCEGREEN         0x8
#define COLORMODIFIER_ONEMINUSSOURCEGREEN 0x9
#define COLORMODIFIER_SOURCEBLUE          0xc
#define COLORMODIFIER_ONEMINUSSOURCEBLUE  0xd

#define ALPHAMODIFIER_SOURCEALPHA         0x0
#define ALPHAMODIFIER_ONEMINUSSOURCEALPHA 0x1
#define ALPHAMODIFIER_SOURCERED           0x2
#define ALPHAMODIFIER_ONEMINUSSOURCERED   0x3
#define ALPHAMODIFIER_SOURCEGREEN         0x4
#define ALPHAMODIFIER_ONEMINUSSOURCEGREEN 0x5
#define ALPHAMODIFIER_SOURCEBLUE          0x6
#define ALPHAMODIFIER_ONEMINUSSOURCEBLUE  0x7

#define OPERATION_REPLACE         0
#define OPERATION_MODULATE        1
#define OPERATION_ADD             2
#define OPERATION_ADDSIGNED       3
#define OPERATION_LERP            4
#define OPERATION_SUBTRACT        5
#define OPERATION_MULTIPLYTHENADD 8
#define OPERATION_ADDTHENMULTIPLY 9

#define COMPAREFUNC_NEVER              0
#define COMPAREFUNC_ALWAYS             1
#define COMPAREFUNC_EQUAL              2
#define COMPAREFUNC_NOTEQUAL           3
#define COMPAREFUNC_LESSTHAN           4
#define COMPAREFUNC_LESSTHANOREQUAL    5
#define COMPAREFUNC_GREATERTHAN        6
#define COMPAREFUNC_GREATERTHANOREQUAL 7

in vec4 o[NUM_VTX_ATTR];
out vec4 color;

uniform bool alphatest_enabled;
uniform int alphatest_func;
uniform float alphatest_ref;

uniform sampler2D tex[3];

uniform vec4 tev_combiner_buffer_color;

struct TEVConfig
{
    bool enabled;
    ivec3 color_sources;
    ivec3 alpha_sources;
    ivec3 color_modifiers;
    ivec3 alpha_modifiers;
    ivec2 color_alpha_op;
    ivec2 color_alpha_multiplier;
    vec4 const_color;
    bvec2 updates_combiner_buffer_color_alpha;
};

uniform TEVConfig tev_cfgs[NUM_TEV_STAGES];

vec4 g_combiner_buffer;
vec4 g_last_tex_env_out;
vec4 g_const_color;

vec4 GetSource(int source) {
    if (source == SOURCE_PRIMARYCOLOR) {
        return o[2];
    } else if (source == SOURCE_PRIMARYFRAGMENTCOLOR) {
        // HACK: Until we implement fragment lighting, use primary_color
        return o[2];
    } else if (source == SOURCE_SECONDARYFRAGMENTCOLOR) {
        // HACK: Until we implement fragment lighting, use zero
        return vec4(0.0, 0.0, 0.0, 0.0);
    } else if (source == SOURCE_TEXTURE0) {
        return texture(tex[0], o[3].xy);
    } else if (source == SOURCE_TEXTURE1) {
        return texture(tex[1], o[3].zw);
    } else if (source == SOURCE_TEXTURE2) {
        // TODO: Unverified
        return texture(tex[2], o[5].zw);
    } else if (source == SOURCE_TEXTURE3) {
        // TODO: no 4th texture?
    } else if (source == SOURCE_PREVIOUSBUFFER) {
        return g_combiner_buffer;
    } else if (source == SOURCE_CONSTANT) {
        return g_const_color;
    } else if (source == SOURCE_PREVIOUS) {
        return g_last_tex_env_out;
    }

    return vec4(0.0);
}

vec3 GetColorModifier(int factor, vec4 color) {
    if (factor == COLORMODIFIER_SOURCECOLOR) {
        return color.rgb;
    } else if (factor == COLORMODIFIER_ONEMINUSSOURCECOLOR) {
        return vec3(1.0) - color.rgb;
    } else if (factor == COLORMODIFIER_SOURCEALPHA) {
        return color.aaa;
    } else if (factor == COLORMODIFIER_ONEMINUSSOURCEALPHA) {
        return vec3(1.0) - color.aaa;
    } else if (factor == COLORMODIFIER_SOURCERED) {
        return color.rrr;
    } else if (factor == COLORMODIFIER_ONEMINUSSOURCERED) {
        return vec3(1.0) - color.rrr;
    } else if (factor == COLORMODIFIER_SOURCEGREEN) {
        return color.ggg;
    } else if (factor == COLORMODIFIER_ONEMINUSSOURCEGREEN) {
        return vec3(1.0) - color.ggg;
    } else if (factor == COLORMODIFIER_SOURCEBLUE) {
        return color.bbb;
    } else if (factor == COLORMODIFIER_ONEMINUSSOURCEBLUE) {
        return vec3(1.0) - color.bbb;
    }

    return vec3(0.0);
}

float GetAlphaModifier(int factor, vec4 color) {
    if (factor == ALPHAMODIFIER_SOURCEALPHA) {
        return color.a;
    } else if (factor == ALPHAMODIFIER_ONEMINUSSOURCEALPHA) {
        return 1.0 - color.a;
    } else if (factor == ALPHAMODIFIER_SOURCERED) {
        return color.r;
    } else if (factor == ALPHAMODIFIER_ONEMINUSSOURCERED) {
        return 1.0 - color.r;
    } else if (factor == ALPHAMODIFIER_SOURCEGREEN) {
        return color.g;
    } else if (factor == ALPHAMODIFIER_ONEMINUSSOURCEGREEN) {
        return 1.0 - color.g;
    } else if (factor == ALPHAMODIFIER_SOURCEBLUE) {
        return color.b;
    } else if (factor == ALPHAMODIFIER_ONEMINUSSOURCEBLUE) {
        return 1.0 - color.b;
    }

    return 0.0;
}

vec3 ColorCombine(int op, vec3 color[3]) {
    if (op == OPERATION_REPLACE) {
        return color[0];
    } else if (op == OPERATION_MODULATE) {
        return color[0] * color[1];
    } else if (op == OPERATION_ADD) {
        return min(color[0] + color[1], 1.0);
    } else if (op == OPERATION_ADDSIGNED) {
        return clamp(color[0] + color[1] - vec3(0.5), 0.0, 1.0);
    } else if (op == OPERATION_LERP) {
        return color[0] * color[2] + color[1] * (vec3(1.0) - color[2]);
    } else if (op == OPERATION_SUBTRACT) {
        return max(color[0] - color[1], 0.0);
    } else if (op == OPERATION_MULTIPLYTHENADD) {
        return min(color[0] * color[1] + color[2], 1.0);
    } else if (op == OPERATION_ADDTHENMULTIPLY) {
        return min(color[0] + color[1], 1.0) * color[2];
    }

    return vec3(0.0);
}

float AlphaCombine(int op, float alpha[3]) {
    if (op == OPERATION_REPLACE) {
        return alpha[0];
    } else if (op == OPERATION_MODULATE) {
        return alpha[0] * alpha[1];
    } else if (op == OPERATION_ADD) {
        return min(alpha[0] + alpha[1], 1.0);
    } else if (op == OPERATION_ADDSIGNED) {
        return clamp(alpha[0] + alpha[1] - 0.5, 0.0, 1.0);
    } else if (op == OPERATION_LERP) {
        return alpha[0] * alpha[2] + alpha[1] * (1.0 - alpha[2]);
    } else if (op == OPERATION_SUBTRACT) {
        return max(alpha[0] - alpha[1], 0.0);
    } else if (op == OPERATION_MULTIPLYTHENADD) {
        return min(alpha[0] * alpha[1] + alpha[2], 1.0);
    } else if (op == OPERATION_ADDTHENMULTIPLY) {
        return min(alpha[0] + alpha[1], 1.0) * alpha[2];
    }

    return 0.0;
}

void main(void) {
    g_combiner_buffer = tev_combiner_buffer_color;

    for (int tex_env_idx = 0; tex_env_idx < NUM_TEV_STAGES; ++tex_env_idx) {
        if (tev_cfgs[tex_env_idx].enabled) {
            g_const_color = tev_cfgs[tex_env_idx].const_color;

            vec3 color_results[3] = vec3[3](GetColorModifier(tev_cfgs[tex_env_idx].color_modifiers.x, GetSource(tev_cfgs[tex_env_idx].color_sources.x)),
                                            GetColorModifier(tev_cfgs[tex_env_idx].color_modifiers.y, GetSource(tev_cfgs[tex_env_idx].color_sources.y)),
                                            GetColorModifier(tev_cfgs[tex_env_idx].color_modifiers.z, GetSource(tev_cfgs[tex_env_idx].color_sources.z)));
            vec3 color_output = ColorCombine(tev_cfgs[tex_env_idx].color_alpha_op.x, color_results);

            float alpha_results[3] = float[3](GetAlphaModifier(tev_cfgs[tex_env_idx].alpha_modifiers.x, GetSource(tev_cfgs[tex_env_idx].alpha_sources.x)),
                                              GetAlphaModifier(tev_cfgs[tex_env_idx].alpha_modifiers.y, GetSource(tev_cfgs[tex_env_idx].alpha_sources.y)),
                                              GetAlphaModifier(tev_cfgs[tex_env_idx].alpha_modifiers.z, GetSource(tev_cfgs[tex_env_idx].alpha_sources.z)));
            float alpha_output = AlphaCombine(tev_cfgs[tex_env_idx].color_alpha_op.y, alpha_results);

            g_last_tex_env_out = vec4(min(color_output * tev_cfgs[tex_env_idx].color_alpha_multiplier.x, 1.0), min(alpha_output * tev_cfgs[tex_env_idx].color_alpha_multiplier.y, 1.0));
        }

        if (tev_cfgs[tex_env_idx].updates_combiner_buffer_color_alpha.x) {
            g_combiner_buffer.rgb = g_last_tex_env_out.rgb;
        }

        if (tev_cfgs[tex_env_idx].updates_combiner_buffer_color_alpha.y) {
            g_combiner_buffer.a = g_last_tex_env_out.a;
        }
    }

    if (alphatest_enabled) {
        if (alphatest_func == COMPAREFUNC_NEVER) {
            discard;
        } else if (alphatest_func == COMPAREFUNC_ALWAYS) {

        } else if (alphatest_func == COMPAREFUNC_EQUAL) {
            if (g_last_tex_env_out.a != alphatest_ref) {
                discard;
            }
        } else if (alphatest_func == COMPAREFUNC_NOTEQUAL) {
            if (g_last_tex_env_out.a == alphatest_ref) {
                discard;
            }
        } else if (alphatest_func == COMPAREFUNC_LESSTHAN) {
            if (g_last_tex_env_out.a >= alphatest_ref) {
                discard;
            }
        } else if (alphatest_func == COMPAREFUNC_LESSTHANOREQUAL) {
            if (g_last_tex_env_out.a > alphatest_ref) {
                discard;
            }
        } else if (alphatest_func == COMPAREFUNC_GREATERTHAN) {
            if (g_last_tex_env_out.a <= alphatest_ref) {
                discard;
            }
        } else if (alphatest_func == COMPAREFUNC_GREATERTHANOREQUAL) {
            if (g_last_tex_env_out.a < alphatest_ref) {
                discard;
            }
        }
    }

    color = g_last_tex_env_out;
}
)";

}
