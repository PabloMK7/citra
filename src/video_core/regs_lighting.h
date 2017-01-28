// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"

namespace Pica {

struct LightingRegs {
    enum class LightingSampler {
        Distribution0 = 0,
        Distribution1 = 1,
        Fresnel = 3,
        ReflectBlue = 4,
        ReflectGreen = 5,
        ReflectRed = 6,
        SpotlightAttenuation = 8,
        DistanceAttenuation = 16,
    };

    /**
    * Pica fragment lighting supports using different LUTs for each lighting component:  Reflectance
    * R, G, and B channels, distribution function for specular components 0 and 1, fresnel factor,
    * and spotlight attenuation.  Furthermore, which LUTs are used for each channel (or whether a
    * channel is enabled at all) is specified by various pre-defined lighting configurations.  With
    * configurations that require more LUTs, more cycles are required on HW to perform lighting
    * computations.
    */
    enum class LightingConfig {
        Config0 = 0, ///< Reflect Red, Distribution 0, Spotlight
        Config1 = 1, ///< Reflect Red, Fresnel, Spotlight
        Config2 = 2, ///< Reflect Red, Distribution 0/1
        Config3 = 3, ///< Distribution 0/1, Fresnel
        Config4 = 4, ///< Reflect Red/Green/Blue, Distribution 0/1, Spotlight
        Config5 = 5, ///< Reflect Red/Green/Blue, Distribution 0, Fresnel, Spotlight
        Config6 = 6, ///< Reflect Red, Distribution 0/1, Fresnel, Spotlight

        Config7 = 8, ///< Reflect Red/Green/Blue, Distribution 0/1, Fresnel, Spotlight
                     ///< NOTE: '8' is intentional, '7' does not appear to be a valid configuration
    };

    /// Selects which lighting components are affected by fresnel
    enum class LightingFresnelSelector {
        None = 0,           ///< Fresnel is disabled
        PrimaryAlpha = 1,   ///< Primary (diffuse) lighting alpha is affected by fresnel
        SecondaryAlpha = 2, ///< Secondary (specular) lighting alpha is affected by fresnel
        Both =
            PrimaryAlpha |
            SecondaryAlpha, ///< Both primary and secondary lighting alphas are affected by fresnel
    };

    /// Factor used to scale the output of a lighting LUT
    enum class LightingScale {
        Scale1 = 0, ///< Scale is 1x
        Scale2 = 1, ///< Scale is 2x
        Scale4 = 2, ///< Scale is 4x
        Scale8 = 3, ///< Scale is 8x

        Scale1_4 = 6, ///< Scale is 0.25x
        Scale1_2 = 7, ///< Scale is 0.5x
    };

    enum class LightingLutInput {
        NH = 0, // Cosine of the angle between the normal and half-angle vectors
        VH = 1, // Cosine of the angle between the view and half-angle vectors
        NV = 2, // Cosine of the angle between the normal and the view vector
        LN = 3, // Cosine of the angle between the light and the normal vectors
    };

    enum class LightingBumpMode : u32 {
        None = 0,
        NormalMap = 1,
        TangentMap = 2,
    };

    union LightColor {
        BitField<0, 10, u32> b;
        BitField<10, 10, u32> g;
        BitField<20, 10, u32> r;

        Math::Vec3f ToVec3f() const {
            // These fields are 10 bits wide, however 255 corresponds to 1.0f for each color
            // component
            return Math::MakeVec((f32)r / 255.f, (f32)g / 255.f, (f32)b / 255.f);
        }
    };

    /// Returns true if the specified lighting sampler is supported by the current Pica lighting
    /// configuration
    static bool IsLightingSamplerSupported(LightingConfig config, LightingSampler sampler) {
        switch (sampler) {
        case LightingSampler::Distribution0:
            return (config != LightingConfig::Config1);

        case LightingSampler::Distribution1:
            return (config != LightingConfig::Config0) && (config != LightingConfig::Config1) &&
                   (config != LightingConfig::Config5);

        case LightingSampler::Fresnel:
            return (config != LightingConfig::Config0) && (config != LightingConfig::Config2) &&
                   (config != LightingConfig::Config4);

        case LightingSampler::ReflectRed:
            return (config != LightingConfig::Config3);

        case LightingSampler::ReflectGreen:
        case LightingSampler::ReflectBlue:
            return (config == LightingConfig::Config4) || (config == LightingConfig::Config5) ||
                   (config == LightingConfig::Config7);
        default:
            UNREACHABLE_MSG("Regs::IsLightingSamplerSupported: Reached "
                            "unreachable section, sampler should be one "
                            "of Distribution0, Distribution1, Fresnel, "
                            "ReflectRed, ReflectGreen or ReflectBlue, instead "
                            "got %i",
                            static_cast<int>(config));
        }
    }

    struct LightSrc {
        LightColor specular_0; // material.specular_0 * light.specular_0
        LightColor specular_1; // material.specular_1 * light.specular_1
        LightColor diffuse;    // material.diffuse * light.diffuse
        LightColor ambient;    // material.ambient * light.ambient

        // Encoded as 16-bit floating point
        union {
            BitField<0, 16, u32> x;
            BitField<16, 16, u32> y;
        };
        union {
            BitField<0, 16, u32> z;
        };

        INSERT_PADDING_WORDS(0x3);

        union {
            BitField<0, 1, u32> directional;
            BitField<1, 1, u32> two_sided_diffuse; // When disabled, clamp dot-product to 0
        } config;

        BitField<0, 20, u32> dist_atten_bias;
        BitField<0, 20, u32> dist_atten_scale;

        INSERT_PADDING_WORDS(0x4);
    };
    static_assert(sizeof(LightSrc) == 0x10 * sizeof(u32), "LightSrc structure must be 0x10 words");

    LightSrc light[8];
    LightColor global_ambient; // Emission + (material.ambient * lighting.ambient)
    INSERT_PADDING_WORDS(0x1);
    BitField<0, 3, u32> max_light_index; // Number of enabled lights - 1

    union {
        BitField<2, 2, LightingFresnelSelector> fresnel_selector;
        BitField<4, 4, LightingConfig> config;
        BitField<22, 2, u32> bump_selector; // 0: Texture 0, 1: Texture 1, 2: Texture 2
        BitField<27, 1, u32> clamp_highlights;
        BitField<28, 2, LightingBumpMode> bump_mode;
        BitField<30, 1, u32> disable_bump_renorm;
    } config0;

    union {
        BitField<16, 1, u32> disable_lut_d0;
        BitField<17, 1, u32> disable_lut_d1;
        BitField<19, 1, u32> disable_lut_fr;
        BitField<20, 1, u32> disable_lut_rr;
        BitField<21, 1, u32> disable_lut_rg;
        BitField<22, 1, u32> disable_lut_rb;

        // Each bit specifies whether distance attenuation should be applied for the corresponding
        // light.
        BitField<24, 1, u32> disable_dist_atten_light_0;
        BitField<25, 1, u32> disable_dist_atten_light_1;
        BitField<26, 1, u32> disable_dist_atten_light_2;
        BitField<27, 1, u32> disable_dist_atten_light_3;
        BitField<28, 1, u32> disable_dist_atten_light_4;
        BitField<29, 1, u32> disable_dist_atten_light_5;
        BitField<30, 1, u32> disable_dist_atten_light_6;
        BitField<31, 1, u32> disable_dist_atten_light_7;
    } config1;

    bool IsDistAttenDisabled(unsigned index) const {
        const unsigned disable[] = {
            config1.disable_dist_atten_light_0, config1.disable_dist_atten_light_1,
            config1.disable_dist_atten_light_2, config1.disable_dist_atten_light_3,
            config1.disable_dist_atten_light_4, config1.disable_dist_atten_light_5,
            config1.disable_dist_atten_light_6, config1.disable_dist_atten_light_7};
        return disable[index] != 0;
    }

    union {
        BitField<0, 8, u32> index; ///< Index at which to set data in the LUT
        BitField<8, 5, u32> type;  ///< Type of LUT for which to set data
    } lut_config;

    BitField<0, 1, u32> disable;
    INSERT_PADDING_WORDS(0x1);

    // When data is written to any of these registers, it gets written to the lookup table of the
    // selected type at the selected index, specified above in the `lut_config` register.  With each
    // write, `lut_config.index` is incremented.  It does not matter which of these registers is
    // written to, the behavior will be the same.
    u32 lut_data[8];

    // These are used to specify if absolute (abs) value should be used for each LUT index.  When
    // abs mode is disabled, LUT indexes are in the range of (-1.0, 1.0).  Otherwise, they are in
    // the range of (0.0, 1.0).
    union {
        BitField<1, 1, u32> disable_d0;
        BitField<5, 1, u32> disable_d1;
        BitField<9, 1, u32> disable_sp;
        BitField<13, 1, u32> disable_fr;
        BitField<17, 1, u32> disable_rb;
        BitField<21, 1, u32> disable_rg;
        BitField<25, 1, u32> disable_rr;
    } abs_lut_input;

    union {
        BitField<0, 3, LightingLutInput> d0;
        BitField<4, 3, LightingLutInput> d1;
        BitField<8, 3, LightingLutInput> sp;
        BitField<12, 3, LightingLutInput> fr;
        BitField<16, 3, LightingLutInput> rb;
        BitField<20, 3, LightingLutInput> rg;
        BitField<24, 3, LightingLutInput> rr;
    } lut_input;

    union {
        BitField<0, 3, LightingScale> d0;
        BitField<4, 3, LightingScale> d1;
        BitField<8, 3, LightingScale> sp;
        BitField<12, 3, LightingScale> fr;
        BitField<16, 3, LightingScale> rb;
        BitField<20, 3, LightingScale> rg;
        BitField<24, 3, LightingScale> rr;

        static float GetScale(LightingScale scale) {
            switch (scale) {
            case LightingScale::Scale1:
                return 1.0f;
            case LightingScale::Scale2:
                return 2.0f;
            case LightingScale::Scale4:
                return 4.0f;
            case LightingScale::Scale8:
                return 8.0f;
            case LightingScale::Scale1_4:
                return 0.25f;
            case LightingScale::Scale1_2:
                return 0.5f;
            }
            return 0.0f;
        }
    } lut_scale;

    INSERT_PADDING_WORDS(0x6);

    union {
        // There are 8 light enable "slots", corresponding to the total number of lights supported
        // by Pica.  For N enabled lights (specified by register 0x1c2, or 'src_num' above), the
        // first N slots below will be set to integers within the range of 0-7, corresponding to the
        // actual light that is enabled for each slot.

        BitField<0, 3, u32> slot_0;
        BitField<4, 3, u32> slot_1;
        BitField<8, 3, u32> slot_2;
        BitField<12, 3, u32> slot_3;
        BitField<16, 3, u32> slot_4;
        BitField<20, 3, u32> slot_5;
        BitField<24, 3, u32> slot_6;
        BitField<28, 3, u32> slot_7;

        unsigned GetNum(unsigned index) const {
            const unsigned enable_slots[] = {slot_0, slot_1, slot_2, slot_3,
                                             slot_4, slot_5, slot_6, slot_7};
            return enable_slots[index];
        }
    } light_enable;

    INSERT_PADDING_WORDS(0x26);
};

static_assert(sizeof(LightingRegs) == 0xC0 * sizeof(u32), "LightingRegs struct has incorrect size");

} // namespace Pica
