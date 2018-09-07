// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "video_core/swrasterizer/lighting.h"

namespace Pica {

static float LookupLightingLut(const Pica::State::Lighting& lighting, std::size_t lut_index,
                               u8 index, float delta) {
    ASSERT_MSG(lut_index < lighting.luts.size(), "Out of range lut");
    ASSERT_MSG(index < lighting.luts[lut_index].size(), "Out of range index");

    const auto& lut = lighting.luts[lut_index][index];

    float lut_value = lut.ToFloat();
    float lut_diff = lut.DiffToFloat();

    return lut_value + lut_diff * delta;
}

std::tuple<Math::Vec4<u8>, Math::Vec4<u8>> ComputeFragmentsColors(
    const Pica::LightingRegs& lighting, const Pica::State::Lighting& lighting_state,
    const Math::Quaternion<float>& normquat, const Math::Vec3<float>& view,
    const Math::Vec4<u8> (&texture_color)[4]) {

    Math::Vec4<float> shadow;
    if (lighting.config0.enable_shadow) {
        shadow = texture_color[lighting.config0.shadow_selector].Cast<float>() / 255.0f;
        if (lighting.config0.shadow_invert) {
            shadow = Math::MakeVec(1.0f, 1.0f, 1.0f, 1.0f) - shadow;
        }
    } else {
        shadow = Math::MakeVec(1.0f, 1.0f, 1.0f, 1.0f);
    }

    Math::Vec3<float> surface_normal;
    Math::Vec3<float> surface_tangent;

    if (lighting.config0.bump_mode != LightingRegs::LightingBumpMode::None) {
        Math::Vec3<float> perturbation =
            texture_color[lighting.config0.bump_selector].xyz().Cast<float>() / 127.5f -
            Math::MakeVec(1.0f, 1.0f, 1.0f);
        if (lighting.config0.bump_mode == LightingRegs::LightingBumpMode::NormalMap) {
            if (!lighting.config0.disable_bump_renorm) {
                const float z_square = 1 - perturbation.xy().Length2();
                perturbation.z = std::sqrt(std::max(z_square, 0.0f));
            }
            surface_normal = perturbation;
            surface_tangent = Math::MakeVec(1.0f, 0.0f, 0.0f);
        } else if (lighting.config0.bump_mode == LightingRegs::LightingBumpMode::TangentMap) {
            surface_normal = Math::MakeVec(0.0f, 0.0f, 1.0f);
            surface_tangent = perturbation;
        } else {
            LOG_ERROR(HW_GPU, "Unknown bump mode {}",
                      static_cast<u32>(lighting.config0.bump_mode.Value()));
        }
    } else {
        surface_normal = Math::MakeVec(0.0f, 0.0f, 1.0f);
        surface_tangent = Math::MakeVec(1.0f, 0.0f, 0.0f);
    }

    // Use the normalized the quaternion when performing the rotation
    auto normal = Math::QuaternionRotate(normquat, surface_normal);
    auto tangent = Math::QuaternionRotate(normquat, surface_tangent);

    Math::Vec4<float> diffuse_sum = {0.0f, 0.0f, 0.0f, 1.0f};
    Math::Vec4<float> specular_sum = {0.0f, 0.0f, 0.0f, 1.0f};

    for (unsigned light_index = 0; light_index <= lighting.max_light_index; ++light_index) {
        unsigned num = lighting.light_enable.GetNum(light_index);
        const auto& light_config = lighting.light[num];

        Math::Vec3<float> refl_value = {};
        Math::Vec3<float> position = {float16::FromRaw(light_config.x).ToFloat32(),
                                      float16::FromRaw(light_config.y).ToFloat32(),
                                      float16::FromRaw(light_config.z).ToFloat32()};
        Math::Vec3<float> light_vector;

        if (light_config.config.directional)
            light_vector = position;
        else
            light_vector = position + view;

        light_vector.Normalize();

        Math::Vec3<float> norm_view = view.Normalized();
        Math::Vec3<float> half_vector = norm_view + light_vector;

        float dist_atten = 1.0f;
        if (!lighting.IsDistAttenDisabled(num)) {
            auto distance = (-view - position).Length();
            float scale = Pica::float20::FromRaw(light_config.dist_atten_scale).ToFloat32();
            float bias = Pica::float20::FromRaw(light_config.dist_atten_bias).ToFloat32();
            std::size_t lut =
                static_cast<std::size_t>(LightingRegs::LightingSampler::DistanceAttenuation) + num;

            float sample_loc = std::clamp(scale * distance + bias, 0.0f, 1.0f);

            u8 lutindex =
                static_cast<u8>(std::clamp(std::floor(sample_loc * 256.0f), 0.0f, 255.0f));
            float delta = sample_loc * 256 - lutindex;
            dist_atten = LookupLightingLut(lighting_state, lut, lutindex, delta);
        }

        auto GetLutValue = [&](LightingRegs::LightingLutInput input, bool abs,
                               LightingRegs::LightingScale scale_enum,
                               LightingRegs::LightingSampler sampler) {
            float result = 0.0f;

            switch (input) {
            case LightingRegs::LightingLutInput::NH:
                result = Math::Dot(normal, half_vector.Normalized());
                break;

            case LightingRegs::LightingLutInput::VH:
                result = Math::Dot(norm_view, half_vector.Normalized());
                break;

            case LightingRegs::LightingLutInput::NV:
                result = Math::Dot(normal, norm_view);
                break;

            case LightingRegs::LightingLutInput::LN:
                result = Math::Dot(light_vector, normal);
                break;

            case LightingRegs::LightingLutInput::SP: {
                Math::Vec3<s32> spot_dir{light_config.spot_x.Value(), light_config.spot_y.Value(),
                                         light_config.spot_z.Value()};
                result = Math::Dot(light_vector, spot_dir.Cast<float>() / 2047.0f);
                break;
            }
            case LightingRegs::LightingLutInput::CP:
                if (lighting.config0.config == LightingRegs::LightingConfig::Config7) {
                    const Math::Vec3<float> norm_half_vector = half_vector.Normalized();
                    const Math::Vec3<float> half_vector_proj =
                        norm_half_vector - normal * Math::Dot(normal, norm_half_vector);
                    result = Math::Dot(half_vector_proj, tangent);
                } else {
                    result = 0.0f;
                }
                break;
            default:
                LOG_CRITICAL(HW_GPU, "Unknown lighting LUT input {}", static_cast<u32>(input));
                UNIMPLEMENTED();
                result = 0.0f;
            }

            u8 index;
            float delta;

            if (abs) {
                if (light_config.config.two_sided_diffuse)
                    result = std::abs(result);
                else
                    result = std::max(result, 0.0f);

                float flr = std::floor(result * 256.0f);
                index = static_cast<u8>(std::clamp(flr, 0.0f, 255.0f));
                delta = result * 256 - index;
            } else {
                float flr = std::floor(result * 128.0f);
                s8 signed_index = static_cast<s8>(std::clamp(flr, -128.0f, 127.0f));
                delta = result * 128.0f - signed_index;
                index = static_cast<u8>(signed_index);
            }

            float scale = lighting.lut_scale.GetScale(scale_enum);
            return scale * LookupLightingLut(lighting_state, static_cast<std::size_t>(sampler),
                                             index, delta);
        };

        // If enabled, compute spot light attenuation value
        float spot_atten = 1.0f;
        if (!lighting.IsSpotAttenDisabled(num) &&
            LightingRegs::IsLightingSamplerSupported(
                lighting.config0.config, LightingRegs::LightingSampler::SpotlightAttenuation)) {
            auto lut = LightingRegs::SpotlightAttenuationSampler(num);
            spot_atten = GetLutValue(lighting.lut_input.sp, lighting.abs_lut_input.disable_sp == 0,
                                     lighting.lut_scale.sp, lut);
        }

        // Specular 0 component
        float d0_lut_value = 1.0f;
        if (lighting.config1.disable_lut_d0 == 0 &&
            LightingRegs::IsLightingSamplerSupported(
                lighting.config0.config, LightingRegs::LightingSampler::Distribution0)) {
            d0_lut_value =
                GetLutValue(lighting.lut_input.d0, lighting.abs_lut_input.disable_d0 == 0,
                            lighting.lut_scale.d0, LightingRegs::LightingSampler::Distribution0);
        }

        Math::Vec3<float> specular_0 = d0_lut_value * light_config.specular_0.ToVec3f();

        // If enabled, lookup ReflectRed value, otherwise, 1.0 is used
        if (lighting.config1.disable_lut_rr == 0 &&
            LightingRegs::IsLightingSamplerSupported(lighting.config0.config,
                                                     LightingRegs::LightingSampler::ReflectRed)) {
            refl_value.x =
                GetLutValue(lighting.lut_input.rr, lighting.abs_lut_input.disable_rr == 0,
                            lighting.lut_scale.rr, LightingRegs::LightingSampler::ReflectRed);
        } else {
            refl_value.x = 1.0f;
        }

        // If enabled, lookup ReflectGreen value, otherwise, ReflectRed value is used
        if (lighting.config1.disable_lut_rg == 0 &&
            LightingRegs::IsLightingSamplerSupported(lighting.config0.config,
                                                     LightingRegs::LightingSampler::ReflectGreen)) {
            refl_value.y =
                GetLutValue(lighting.lut_input.rg, lighting.abs_lut_input.disable_rg == 0,
                            lighting.lut_scale.rg, LightingRegs::LightingSampler::ReflectGreen);
        } else {
            refl_value.y = refl_value.x;
        }

        // If enabled, lookup ReflectBlue value, otherwise, ReflectRed value is used
        if (lighting.config1.disable_lut_rb == 0 &&
            LightingRegs::IsLightingSamplerSupported(lighting.config0.config,
                                                     LightingRegs::LightingSampler::ReflectBlue)) {
            refl_value.z =
                GetLutValue(lighting.lut_input.rb, lighting.abs_lut_input.disable_rb == 0,
                            lighting.lut_scale.rb, LightingRegs::LightingSampler::ReflectBlue);
        } else {
            refl_value.z = refl_value.x;
        }

        // Specular 1 component
        float d1_lut_value = 1.0f;
        if (lighting.config1.disable_lut_d1 == 0 &&
            LightingRegs::IsLightingSamplerSupported(
                lighting.config0.config, LightingRegs::LightingSampler::Distribution1)) {
            d1_lut_value =
                GetLutValue(lighting.lut_input.d1, lighting.abs_lut_input.disable_d1 == 0,
                            lighting.lut_scale.d1, LightingRegs::LightingSampler::Distribution1);
        }

        Math::Vec3<float> specular_1 =
            d1_lut_value * refl_value * light_config.specular_1.ToVec3f();

        // Fresnel
        // Note: only the last entry in the light slots applies the Fresnel factor
        if (light_index == lighting.max_light_index && lighting.config1.disable_lut_fr == 0 &&
            LightingRegs::IsLightingSamplerSupported(lighting.config0.config,
                                                     LightingRegs::LightingSampler::Fresnel)) {

            float lut_value =
                GetLutValue(lighting.lut_input.fr, lighting.abs_lut_input.disable_fr == 0,
                            lighting.lut_scale.fr, LightingRegs::LightingSampler::Fresnel);

            // Enabled for diffuse lighting alpha component
            if (lighting.config0.enable_primary_alpha) {
                diffuse_sum.a() = lut_value;
            }

            // Enabled for the specular lighting alpha component
            if (lighting.config0.enable_secondary_alpha) {
                specular_sum.a() = lut_value;
            }
        }

        auto dot_product = Math::Dot(light_vector, normal);
        if (light_config.config.two_sided_diffuse)
            dot_product = std::abs(dot_product);
        else
            dot_product = std::max(dot_product, 0.0f);

        float clamp_highlights = 1.0f;
        if (lighting.config0.clamp_highlights) {
            clamp_highlights = dot_product == 0.0f ? 0.0f : 1.0f;
        }

        if (light_config.config.geometric_factor_0 || light_config.config.geometric_factor_1) {
            float geo_factor = half_vector.Length2();
            geo_factor = geo_factor == 0.0f ? 0.0f : std::min(dot_product / geo_factor, 1.0f);
            if (light_config.config.geometric_factor_0) {
                specular_0 *= geo_factor;
            }
            if (light_config.config.geometric_factor_1) {
                specular_1 *= geo_factor;
            }
        }

        auto diffuse =
            (light_config.diffuse.ToVec3f() * dot_product + light_config.ambient.ToVec3f()) *
            dist_atten * spot_atten;
        auto specular = (specular_0 + specular_1) * clamp_highlights * dist_atten * spot_atten;

        if (!lighting.IsShadowDisabled(num)) {
            if (lighting.config0.shadow_primary) {
                diffuse = diffuse * shadow.xyz();
            }
            if (lighting.config0.shadow_secondary) {
                specular = specular * shadow.xyz();
            }
        }

        diffuse_sum += Math::MakeVec(diffuse, 0.0f);
        specular_sum += Math::MakeVec(specular, 0.0f);
    }

    if (lighting.config0.shadow_alpha) {
        // Alpha shadow also uses the Fresnel selecotr to determine which alpha to apply
        // Enabled for diffuse lighting alpha component
        if (lighting.config0.enable_primary_alpha) {
            diffuse_sum.a() *= shadow.w;
        }

        // Enabled for the specular lighting alpha component
        if (lighting.config0.enable_secondary_alpha) {
            specular_sum.a() *= shadow.w;
        }
    }

    diffuse_sum += Math::MakeVec(lighting.global_ambient.ToVec3f(), 0.0f);

    auto diffuse = Math::MakeVec<float>(std::clamp(diffuse_sum.x, 0.0f, 1.0f) * 255,
                                        std::clamp(diffuse_sum.y, 0.0f, 1.0f) * 255,
                                        std::clamp(diffuse_sum.z, 0.0f, 1.0f) * 255,
                                        std::clamp(diffuse_sum.w, 0.0f, 1.0f) * 255)
                       .Cast<u8>();
    auto specular = Math::MakeVec<float>(std::clamp(specular_sum.x, 0.0f, 1.0f) * 255,
                                         std::clamp(specular_sum.y, 0.0f, 1.0f) * 255,
                                         std::clamp(specular_sum.z, 0.0f, 1.0f) * 255,
                                         std::clamp(specular_sum.w, 0.0f, 1.0f) * 255)
                        .Cast<u8>();
    return std::make_tuple(diffuse, specular);
}

} // namespace Pica
