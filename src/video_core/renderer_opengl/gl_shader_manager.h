// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <glad/glad.h>
#include "video_core/regs_lighting.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_shader_gen.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/pica_to_gl.h"

namespace OpenGL {

enum class UniformBindings : GLuint { Common, VS, GS };

struct LightSrc {
    alignas(16) GLvec3 specular_0;
    alignas(16) GLvec3 specular_1;
    alignas(16) GLvec3 diffuse;
    alignas(16) GLvec3 ambient;
    alignas(16) GLvec3 position;
    alignas(16) GLvec3 spot_direction; // negated
    GLfloat dist_atten_bias;
    GLfloat dist_atten_scale;
};

/// Uniform structure for the Uniform Buffer Object, all vectors must be 16-byte aligned
// NOTE: Always keep a vec4 at the end. The GL spec is not clear wether the alignment at
//       the end of a uniform block is included in UNIFORM_BLOCK_DATA_SIZE or not.
//       Not following that rule will cause problems on some AMD drivers.
struct UniformData {
    GLint framebuffer_scale;
    GLint alphatest_ref;
    GLfloat depth_scale;
    GLfloat depth_offset;
    GLfloat shadow_bias_constant;
    GLfloat shadow_bias_linear;
    GLint scissor_x1;
    GLint scissor_y1;
    GLint scissor_x2;
    GLint scissor_y2;
    GLint fog_lut_offset;
    GLint proctex_noise_lut_offset;
    GLint proctex_color_map_offset;
    GLint proctex_alpha_map_offset;
    GLint proctex_lut_offset;
    GLint proctex_diff_lut_offset;
    GLfloat proctex_bias;
    GLint shadow_texture_bias;
    alignas(16) GLivec4 lighting_lut_offset[Pica::LightingRegs::NumLightingSampler / 4];
    alignas(16) GLvec3 fog_color;
    alignas(8) GLvec2 proctex_noise_f;
    alignas(8) GLvec2 proctex_noise_a;
    alignas(8) GLvec2 proctex_noise_p;
    alignas(16) GLvec3 lighting_global_ambient;
    LightSrc light_src[8];
    alignas(16) GLvec4 const_color[6]; // A vec4 color for each of the six tev stages
    alignas(16) GLvec4 tev_combiner_buffer_color;
    alignas(16) GLvec4 clip_coef;
};

static_assert(
    sizeof(UniformData) == 0x4F0,
    "The size of the UniformData structure has changed, update the structure in the shader");
static_assert(sizeof(UniformData) < 16384,
              "UniformData structure must be less than 16kb as per the OpenGL spec");

/// Uniform struct for the Uniform Buffer Object that contains PICA vertex/geometry shader uniforms.
// NOTE: the same rule from UniformData also applies here.
struct PicaUniformsData {
    void SetFromRegs(const Pica::ShaderRegs& regs, const Pica::Shader::ShaderSetup& setup);

    struct BoolAligned {
        alignas(16) GLint b;
    };

    std::array<BoolAligned, 16> bools;
    alignas(16) std::array<GLuvec4, 4> i;
    alignas(16) std::array<GLvec4, 96> f;
};

struct VSUniformData {
    PicaUniformsData uniforms;
};
static_assert(
    sizeof(VSUniformData) == 1856,
    "The size of the VSUniformData structure has changed, update the structure in the shader");
static_assert(sizeof(VSUniformData) < 16384,
              "VSUniformData structure must be less than 16kb as per the OpenGL spec");

/// A class that manage different shader stages and configures them with given config data.
class ShaderProgramManager {
public:
    ShaderProgramManager(bool separable, bool is_amd);
    ~ShaderProgramManager();

    bool UseProgrammableVertexShader(const PicaVSConfig& config,
                                     const Pica::Shader::ShaderSetup setup);

    void UseTrivialVertexShader();

    void UseFixedGeometryShader(const PicaFixedGSConfig& config);

    void UseTrivialGeometryShader();

    void UseFragmentShader(const PicaFSConfig& config);

    void ApplyTo(OpenGLState& state);

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace OpenGL
