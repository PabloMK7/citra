// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "common/vector_math.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/regs_lighting.h"

namespace Core {
class System;
}

namespace Frontend {
class EmuWindow;
}

namespace Pica {
struct Regs;
struct ShaderRegs;
} // namespace Pica

namespace Pica::Shader {
struct ShaderSetup;
}

namespace OpenGL {

enum class UniformBindings : u32 { Common, VS, GS };

struct LightSrc {
    alignas(16) Common::Vec3f specular_0;
    alignas(16) Common::Vec3f specular_1;
    alignas(16) Common::Vec3f diffuse;
    alignas(16) Common::Vec3f ambient;
    alignas(16) Common::Vec3f position;
    alignas(16) Common::Vec3f spot_direction; // negated
    float dist_atten_bias;
    float dist_atten_scale;
};

/// Uniform structure for the Uniform Buffer Object, all vectors must be 16-byte aligned
// NOTE: Always keep a vec4 at the end. The GL spec is not clear wether the alignment at
//       the end of a uniform block is included in UNIFORM_BLOCK_DATA_SIZE or not.
//       Not following that rule will cause problems on some AMD drivers.
struct UniformData {
    int framebuffer_scale;
    int alphatest_ref;
    float depth_scale;
    float depth_offset;
    float shadow_bias_constant;
    float shadow_bias_linear;
    int scissor_x1;
    int scissor_y1;
    int scissor_x2;
    int scissor_y2;
    int fog_lut_offset;
    int proctex_noise_lut_offset;
    int proctex_color_map_offset;
    int proctex_alpha_map_offset;
    int proctex_lut_offset;
    int proctex_diff_lut_offset;
    float proctex_bias;
    int shadow_texture_bias;
    alignas(16) Common::Vec4i lighting_lut_offset[Pica::LightingRegs::NumLightingSampler / 4];
    alignas(16) Common::Vec3f fog_color;
    alignas(8) Common::Vec2f proctex_noise_f;
    alignas(8) Common::Vec2f proctex_noise_a;
    alignas(8) Common::Vec2f proctex_noise_p;
    alignas(16) Common::Vec3f lighting_global_ambient;
    LightSrc light_src[8];
    alignas(16) Common::Vec4f const_color[6]; // A vec4 color for each of the six tev stages
    alignas(16) Common::Vec4f tev_combiner_buffer_color;
    alignas(16) Common::Vec4f clip_coef;
};

static_assert(sizeof(UniformData) == 0x4F0,
              "The size of the UniformData does not match the structure in the shader");
static_assert(sizeof(UniformData) < 16384,
              "UniformData structure must be less than 16kb as per the OpenGL spec");

/// Uniform struct for the Uniform Buffer Object that contains PICA vertex/geometry shader uniforms.
// NOTE: the same rule from UniformData also applies here.
struct PicaUniformsData {
    void SetFromRegs(const Pica::ShaderRegs& regs, const Pica::Shader::ShaderSetup& setup);

    struct BoolAligned {
        alignas(16) int b;
    };

    std::array<BoolAligned, 16> bools;
    alignas(16) std::array<Common::Vec4u, 4> i;
    alignas(16) std::array<Common::Vec4f, 96> f;
};

struct VSUniformData {
    PicaUniformsData uniforms;
};
static_assert(sizeof(VSUniformData) == 1856,
              "The size of the VSUniformData does not match the structure in the shader");
static_assert(sizeof(VSUniformData) < 16384,
              "VSUniformData structure must be less than 16kb as per the OpenGL spec");

class OpenGLState;

/// A class that manage different shader stages and configures them with given config data.
class ShaderProgramManager {
public:
    ShaderProgramManager(Frontend::EmuWindow& emu_window_, bool separable);
    ~ShaderProgramManager();

    void LoadDiskCache(const std::atomic_bool& stop_loading,
                       const VideoCore::DiskResourceLoadCallback& callback);

    bool UseProgrammableVertexShader(const Pica::Regs& config, Pica::Shader::ShaderSetup& setup);

    void UseTrivialVertexShader();

    void UseFixedGeometryShader(const Pica::Regs& regs);

    void UseTrivialGeometryShader();

    void UseFragmentShader(const Pica::Regs& config);

    void ApplyTo(OpenGLState& state);

private:
    class Impl;
    std::unique_ptr<Impl> impl;

    Frontend::EmuWindow& emu_window;
};
} // namespace OpenGL
