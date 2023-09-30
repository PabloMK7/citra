// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_set.h"
#include "common/logging/log.h"
#include "video_core/shader/generator/shader_gen.h"
#include "video_core/video_core.h"

namespace Pica::Shader::Generator {

PicaFSConfig::PicaFSConfig(const Pica::Regs& regs, bool has_fragment_shader_interlock,
                           bool emulate_logic_op, bool emulate_custom_border_color,
                           bool emulate_blend_minmax_factor, bool use_custom_normal_map) {
    state.scissor_test_mode.Assign(regs.rasterizer.scissor_test.mode);

    state.depthmap_enable.Assign(regs.rasterizer.depthmap_enable);

    state.alpha_test_func.Assign(regs.framebuffer.output_merger.alpha_test.enable
                                     ? regs.framebuffer.output_merger.alpha_test.func.Value()
                                     : Pica::FramebufferRegs::CompareFunc::Always);

    state.texture0_type.Assign(regs.texturing.texture0.type);

    state.texture2_use_coord1.Assign(regs.texturing.main_config.texture2_use_coord1 != 0);

    const auto pica_textures = regs.texturing.GetTextures();
    for (u32 tex_index = 0; tex_index < 3; tex_index++) {
        const auto config = pica_textures[tex_index].config;
        state.texture_border_color[tex_index].enable_s.Assign(
            emulate_custom_border_color &&
            config.wrap_s == Pica::TexturingRegs::TextureConfig::WrapMode::ClampToBorder);
        state.texture_border_color[tex_index].enable_t.Assign(
            emulate_custom_border_color &&
            config.wrap_t == Pica::TexturingRegs::TextureConfig::WrapMode::ClampToBorder);
    }

    // Emulate logic op in the shader if not supported. This is mostly for mobile GPUs
    const bool needs_emulate_logic_op =
        emulate_logic_op && !regs.framebuffer.output_merger.alphablend_enable;

    state.emulate_logic_op.Assign(needs_emulate_logic_op);
    if (needs_emulate_logic_op) {
        state.logic_op.Assign(regs.framebuffer.output_merger.logic_op);
    } else {
        state.logic_op.Assign(Pica::FramebufferRegs::LogicOp::NoOp);
    }

    // Copy relevant tev stages fields.
    // We don't sync const_color here because of the high variance, it is a
    // shader uniform instead.
    const auto& tev_stages = regs.texturing.GetTevStages();
    DEBUG_ASSERT(state.tev_stages.size() == tev_stages.size());
    for (std::size_t i = 0; i < tev_stages.size(); i++) {
        const auto& tev_stage = tev_stages[i];
        state.tev_stages[i].sources_raw = tev_stage.sources_raw;
        state.tev_stages[i].modifiers_raw = tev_stage.modifiers_raw;
        state.tev_stages[i].ops_raw = tev_stage.ops_raw;
        state.tev_stages[i].scales_raw = tev_stage.scales_raw;
        if (tev_stage.color_op == Pica::TexturingRegs::TevStageConfig::Operation::Dot3_RGBA) {
            state.tev_stages[i].sources_raw &= 0xFFF;
            state.tev_stages[i].modifiers_raw &= 0xFFF;
            state.tev_stages[i].ops_raw &= 0xF;
        }
    }

    state.fog_mode.Assign(regs.texturing.fog_mode);
    state.fog_flip.Assign(regs.texturing.fog_flip != 0);

    state.combiner_buffer_input.Assign(
        regs.texturing.tev_combiner_buffer_input.update_mask_rgb.Value() |
        regs.texturing.tev_combiner_buffer_input.update_mask_a.Value() << 4);

    // Fragment lighting
    state.lighting.enable.Assign(!regs.lighting.disable);
    if (state.lighting.enable) {
        state.lighting.src_num.Assign(regs.lighting.max_light_index + 1);

        for (u32 light_index = 0; light_index < state.lighting.src_num; ++light_index) {
            const u32 num = regs.lighting.light_enable.GetNum(light_index);
            const auto& light = regs.lighting.light[num];
            state.lighting.light[light_index].num.Assign(num);
            state.lighting.light[light_index].directional.Assign(light.config.directional != 0);
            state.lighting.light[light_index].two_sided_diffuse.Assign(
                light.config.two_sided_diffuse != 0);
            state.lighting.light[light_index].geometric_factor_0.Assign(
                light.config.geometric_factor_0 != 0);
            state.lighting.light[light_index].geometric_factor_1.Assign(
                light.config.geometric_factor_1 != 0);
            state.lighting.light[light_index].dist_atten_enable.Assign(
                !regs.lighting.IsDistAttenDisabled(num));
            state.lighting.light[light_index].spot_atten_enable.Assign(
                !regs.lighting.IsSpotAttenDisabled(num));
            state.lighting.light[light_index].shadow_enable.Assign(
                !regs.lighting.IsShadowDisabled(num));
        }

        state.lighting.lut_d0.enable.Assign(regs.lighting.config1.disable_lut_d0 == 0);
        if (state.lighting.lut_d0.enable) {
            state.lighting.lut_d0.abs_input.Assign(regs.lighting.abs_lut_input.disable_d0 == 0);
            state.lighting.lut_d0.type.Assign(regs.lighting.lut_input.d0.Value());
            state.lighting.lut_d0.scale =
                regs.lighting.lut_scale.GetScale(regs.lighting.lut_scale.d0);
        }

        state.lighting.lut_d1.enable.Assign(regs.lighting.config1.disable_lut_d1 == 0);
        if (state.lighting.lut_d1.enable) {
            state.lighting.lut_d1.abs_input.Assign(regs.lighting.abs_lut_input.disable_d1 == 0);
            state.lighting.lut_d1.type.Assign(regs.lighting.lut_input.d1.Value());
            state.lighting.lut_d1.scale =
                regs.lighting.lut_scale.GetScale(regs.lighting.lut_scale.d1);
        }

        // this is a dummy field due to lack of the corresponding register
        state.lighting.lut_sp.enable.Assign(1);
        state.lighting.lut_sp.abs_input.Assign(regs.lighting.abs_lut_input.disable_sp == 0);
        state.lighting.lut_sp.type.Assign(regs.lighting.lut_input.sp.Value());
        state.lighting.lut_sp.scale = regs.lighting.lut_scale.GetScale(regs.lighting.lut_scale.sp);

        state.lighting.lut_fr.enable.Assign(regs.lighting.config1.disable_lut_fr == 0);
        if (state.lighting.lut_fr.enable) {
            state.lighting.lut_fr.abs_input.Assign(regs.lighting.abs_lut_input.disable_fr == 0);
            state.lighting.lut_fr.type.Assign(regs.lighting.lut_input.fr.Value());
            state.lighting.lut_fr.scale =
                regs.lighting.lut_scale.GetScale(regs.lighting.lut_scale.fr);
        }

        state.lighting.lut_rr.enable.Assign(regs.lighting.config1.disable_lut_rr == 0);
        if (state.lighting.lut_rr.enable) {
            state.lighting.lut_rr.abs_input.Assign(regs.lighting.abs_lut_input.disable_rr == 0);
            state.lighting.lut_rr.type.Assign(regs.lighting.lut_input.rr.Value());
            state.lighting.lut_rr.scale =
                regs.lighting.lut_scale.GetScale(regs.lighting.lut_scale.rr);
        }

        state.lighting.lut_rg.enable.Assign(regs.lighting.config1.disable_lut_rg == 0);
        if (state.lighting.lut_rg.enable) {
            state.lighting.lut_rg.abs_input.Assign(regs.lighting.abs_lut_input.disable_rg == 0);
            state.lighting.lut_rg.type.Assign(regs.lighting.lut_input.rg.Value());
            state.lighting.lut_rg.scale =
                regs.lighting.lut_scale.GetScale(regs.lighting.lut_scale.rg);
        }

        state.lighting.lut_rb.enable.Assign(regs.lighting.config1.disable_lut_rb == 0);
        if (state.lighting.lut_rb.enable) {
            state.lighting.lut_rb.abs_input.Assign(regs.lighting.abs_lut_input.disable_rb == 0);
            state.lighting.lut_rb.type.Assign(regs.lighting.lut_input.rb.Value());
            state.lighting.lut_rb.scale =
                regs.lighting.lut_scale.GetScale(regs.lighting.lut_scale.rb);
        }

        state.lighting.config.Assign(regs.lighting.config0.config);
        state.lighting.enable_primary_alpha.Assign(regs.lighting.config0.enable_primary_alpha);
        state.lighting.enable_secondary_alpha.Assign(regs.lighting.config0.enable_secondary_alpha);
        state.lighting.bump_mode.Assign(regs.lighting.config0.bump_mode);
        state.lighting.bump_selector.Assign(regs.lighting.config0.bump_selector);
        state.lighting.bump_renorm.Assign(regs.lighting.config0.disable_bump_renorm == 0);
        state.lighting.clamp_highlights.Assign(regs.lighting.config0.clamp_highlights != 0);

        state.lighting.enable_shadow.Assign(regs.lighting.config0.enable_shadow != 0);
        if (state.lighting.enable_shadow) {
            state.lighting.shadow_primary.Assign(regs.lighting.config0.shadow_primary != 0);
            state.lighting.shadow_secondary.Assign(regs.lighting.config0.shadow_secondary != 0);
            state.lighting.shadow_invert.Assign(regs.lighting.config0.shadow_invert != 0);
            state.lighting.shadow_alpha.Assign(regs.lighting.config0.shadow_alpha != 0);
            state.lighting.shadow_selector.Assign(regs.lighting.config0.shadow_selector);
        }
    }

    state.proctex.enable.Assign(regs.texturing.main_config.texture3_enable);
    if (state.proctex.enable) {
        state.proctex.coord.Assign(regs.texturing.main_config.texture3_coordinates);
        state.proctex.u_clamp.Assign(regs.texturing.proctex.u_clamp);
        state.proctex.v_clamp.Assign(regs.texturing.proctex.v_clamp);
        state.proctex.color_combiner.Assign(regs.texturing.proctex.color_combiner);
        state.proctex.alpha_combiner.Assign(regs.texturing.proctex.alpha_combiner);
        state.proctex.separate_alpha.Assign(regs.texturing.proctex.separate_alpha);
        state.proctex.noise_enable.Assign(regs.texturing.proctex.noise_enable);
        state.proctex.u_shift.Assign(regs.texturing.proctex.u_shift);
        state.proctex.v_shift.Assign(regs.texturing.proctex.v_shift);
        state.proctex.lut_width = regs.texturing.proctex_lut.width;
        state.proctex.lut_offset0 = regs.texturing.proctex_lut_offset.level0;
        state.proctex.lut_offset1 = regs.texturing.proctex_lut_offset.level1;
        state.proctex.lut_offset2 = regs.texturing.proctex_lut_offset.level2;
        state.proctex.lut_offset3 = regs.texturing.proctex_lut_offset.level3;
        state.proctex.lod_min = regs.texturing.proctex_lut.lod_min;
        state.proctex.lod_max = regs.texturing.proctex_lut.lod_max;
        state.proctex.lut_filter.Assign(regs.texturing.proctex_lut.filter);
    }

    const auto alpha_eq = regs.framebuffer.output_merger.alpha_blending.blend_equation_a.Value();
    const auto rgb_eq = regs.framebuffer.output_merger.alpha_blending.blend_equation_rgb.Value();
    if (emulate_blend_minmax_factor && regs.framebuffer.output_merger.alphablend_enable) {
        if (rgb_eq == Pica::FramebufferRegs::BlendEquation::Max ||
            rgb_eq == Pica::FramebufferRegs::BlendEquation::Min) {
            state.rgb_blend.emulate_blending = true;
            state.rgb_blend.eq = rgb_eq;
            state.rgb_blend.src_factor =
                regs.framebuffer.output_merger.alpha_blending.factor_source_rgb;
            state.rgb_blend.dst_factor =
                regs.framebuffer.output_merger.alpha_blending.factor_dest_rgb;
        }
        if (alpha_eq == Pica::FramebufferRegs::BlendEquation::Max ||
            alpha_eq == Pica::FramebufferRegs::BlendEquation::Min) {
            state.alpha_blend.emulate_blending = true;
            state.alpha_blend.eq = alpha_eq;
            state.alpha_blend.src_factor =
                regs.framebuffer.output_merger.alpha_blending.factor_source_a;
            state.alpha_blend.dst_factor =
                regs.framebuffer.output_merger.alpha_blending.factor_dest_a;
        }
    }

    state.shadow_rendering.Assign(regs.framebuffer.output_merger.fragment_operation_mode ==
                                  Pica::FramebufferRegs::FragmentOperationMode::Shadow);
    state.shadow_texture_orthographic.Assign(regs.texturing.shadow.orthographic != 0);

    // We only need fragment shader interlock when shadow rendering.
    state.use_fragment_shader_interlock.Assign(state.shadow_rendering &&
                                               has_fragment_shader_interlock);
    state.use_custom_normal_map.Assign(use_custom_normal_map);
}

void PicaGSConfigState::Init(const Pica::Regs& regs, bool use_clip_planes_) {
    use_clip_planes = use_clip_planes_;

    vs_output_attributes = Common::BitSet<u32>(regs.vs.output_mask).Count();
    gs_output_attributes = vs_output_attributes;

    semantic_maps.fill({16, 0});
    for (u32 attrib = 0; attrib < regs.rasterizer.vs_output_total; ++attrib) {
        const std::array semantics{
            regs.rasterizer.vs_output_attributes[attrib].map_x.Value(),
            regs.rasterizer.vs_output_attributes[attrib].map_y.Value(),
            regs.rasterizer.vs_output_attributes[attrib].map_z.Value(),
            regs.rasterizer.vs_output_attributes[attrib].map_w.Value(),
        };
        for (u32 comp = 0; comp < 4; ++comp) {
            const auto semantic = semantics[comp];
            if (static_cast<std::size_t>(semantic) < 24) {
                semantic_maps[static_cast<std::size_t>(semantic)] = {attrib, comp};
            } else if (semantic != Pica::RasterizerRegs::VSOutputAttributes::INVALID) {
                LOG_ERROR(Render, "Invalid/unknown semantic id: {}", semantic);
            }
        }
    }
}

void PicaVSConfigState::Init(const Pica::Regs& regs, Pica::Shader::ShaderSetup& setup,
                             bool use_clip_planes_, bool use_geometry_shader_) {
    use_clip_planes = use_clip_planes_;
    use_geometry_shader = use_geometry_shader_;

    program_hash = setup.GetProgramCodeHash();
    swizzle_hash = setup.GetSwizzleDataHash();
    main_offset = regs.vs.main_offset;
    sanitize_mul = VideoCore::g_hw_shader_accurate_mul;

    num_outputs = 0;
    load_flags.fill(AttribLoadFlags::Float);
    output_map.fill(16);

    for (int reg : Common::BitSet<u32>(regs.vs.output_mask)) {
        output_map[reg] = num_outputs++;
    }

    if (!use_geometry_shader_) {
        gs_state.Init(regs, use_clip_planes_);
    }
}

PicaVSConfig::PicaVSConfig(const Pica::Regs& regs, Pica::Shader::ShaderSetup& setup,
                           bool use_clip_planes_, bool use_geometry_shader_) {
    state.Init(regs, setup, use_clip_planes_, use_geometry_shader_);
}

PicaFixedGSConfig::PicaFixedGSConfig(const Pica::Regs& regs, bool use_clip_planes_) {
    state.Init(regs, use_clip_planes_);
}

} // namespace Pica::Shader::Generator
