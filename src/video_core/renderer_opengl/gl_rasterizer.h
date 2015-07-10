// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "common/common_types.h"

#include "video_core/hwrasterizer_base.h"
#include "video_core/vertex_shader.h"

#include "gl_state.h"
#include "gl_rasterizer_cache.h"

class RasterizerOpenGL : public HWRasterizer {
public:

    RasterizerOpenGL();
    ~RasterizerOpenGL() override;

    /// Initialize API-specific GPU objects
    void InitObjects() override;

    /// Reset the rasterizer, such as flushing all caches and updating all state
    void Reset() override;

    /// Queues the primitive formed by the given vertices for rendering
    void AddTriangle(const Pica::VertexShader::OutputVertex& v0,
                     const Pica::VertexShader::OutputVertex& v1,
                     const Pica::VertexShader::OutputVertex& v2) override;

    /// Draw the current batch of triangles
    void DrawTriangles() override;

    /// Commit the rasterizer's framebuffer contents immediately to the current 3DS memory framebuffer
    void CommitFramebuffer() override;

    /// Notify rasterizer that the specified PICA register has been changed
    void NotifyPicaRegisterChanged(u32 id) override;

    /// Notify rasterizer that the specified 3DS memory region will be read from after this notification
    void NotifyPreRead(PAddr addr, u32 size) override;

    /// Notify rasterizer that a 3DS memory region has been changed
    void NotifyFlush(PAddr addr, u32 size) override;

private:
    /// Structure used for managing texture environment states
    struct TEVConfigUniforms {
        GLuint enabled;
        GLuint color_sources;
        GLuint alpha_sources;
        GLuint color_modifiers;
        GLuint alpha_modifiers;
        GLuint color_alpha_op;
        GLuint color_alpha_multiplier;
        GLuint const_color;
        GLuint updates_combiner_buffer_color_alpha;
    };

    /// Structure used for storing information about color textures
    struct TextureInfo {
        OGLTexture texture;
        GLsizei width;
        GLsizei height;
        Pica::Regs::ColorFormat format;
        GLenum gl_format;
        GLenum gl_type;
    };

    /// Structure used for storing information about depth textures
    struct DepthTextureInfo {
        OGLTexture texture;
        GLsizei width;
        GLsizei height;
        Pica::Regs::DepthFormat format;
        GLenum gl_format;
        GLenum gl_type;
    };

    /// Structure that the hardware rendered vertices are composed of
    struct HardwareVertex {
        HardwareVertex(const Pica::VertexShader::OutputVertex& v) {
            position[0] = v.pos.x.ToFloat32();
            position[1] = v.pos.y.ToFloat32();
            position[2] = v.pos.z.ToFloat32();
            position[3] = v.pos.w.ToFloat32();
            color[0] = v.color.x.ToFloat32();
            color[1] = v.color.y.ToFloat32();
            color[2] = v.color.z.ToFloat32();
            color[3] = v.color.w.ToFloat32();
            tex_coord0[0] = v.tc0.x.ToFloat32();
            tex_coord0[1] = v.tc0.y.ToFloat32();
            tex_coord1[0] = v.tc1.x.ToFloat32();
            tex_coord1[1] = v.tc1.y.ToFloat32();
            tex_coord2[0] = v.tc2.x.ToFloat32();
            tex_coord2[1] = v.tc2.y.ToFloat32();
        }

        GLfloat position[4];
        GLfloat color[4];
        GLfloat tex_coord0[2];
        GLfloat tex_coord1[2];
        GLfloat tex_coord2[2];
    };

    /// Reconfigure the OpenGL color texture to use the given format and dimensions
    void ReconfigureColorTexture(TextureInfo& texture, Pica::Regs::ColorFormat format, u32 width, u32 height);

    /// Reconfigure the OpenGL depth texture to use the given format and dimensions
    void ReconfigureDepthTexture(DepthTextureInfo& texture, Pica::Regs::DepthFormat format, u32 width, u32 height);

    /// Syncs the state and contents of the OpenGL framebuffer to match the current PICA framebuffer
    void SyncFramebuffer();

    /// Syncs the cull mode to match the PICA register
    void SyncCullMode();

    /// Syncs the blend enabled status to match the PICA register
    void SyncBlendEnabled();

    /// Syncs the blend functions to match the PICA register
    void SyncBlendFuncs();

    /// Syncs the blend color to match the PICA register
    void SyncBlendColor();

    /// Syncs the alpha test states to match the PICA register
    void SyncAlphaTest();

    /// Syncs the logic op states to match the PICA register
    void SyncLogicOp();

    /// Syncs the stencil test states to match the PICA register
    void SyncStencilTest();

    /// Syncs the depth test states to match the PICA register
    void SyncDepthTest();

    /// Syncs the specified TEV stage's color and alpha sources to match the PICA register
    void SyncTevSources(unsigned stage_index, const Pica::Regs::TevStageConfig& config);

    /// Syncs the specified TEV stage's color and alpha modifiers to match the PICA register
    void SyncTevModifiers(unsigned stage_index, const Pica::Regs::TevStageConfig& config);

    /// Syncs the specified TEV stage's color and alpha combiner operations to match the PICA register
    void SyncTevOps(unsigned stage_index, const Pica::Regs::TevStageConfig& config);

    /// Syncs the specified TEV stage's constant color to match the PICA register
    void SyncTevColor(unsigned stage_index, const Pica::Regs::TevStageConfig& config);

    /// Syncs the specified TEV stage's color and alpha multipliers to match the PICA register
    void SyncTevMultipliers(unsigned stage_index, const Pica::Regs::TevStageConfig& config);

    /// Syncs the TEV combiner color buffer to match the PICA register
    void SyncCombinerColor();

    /// Syncs the TEV combiner write flags to match the PICA register
    void SyncCombinerWriteFlags();

    /// Syncs the remaining OpenGL drawing state to match the current PICA state
    void SyncDrawState();

    /// Copies the 3DS color framebuffer into the OpenGL color framebuffer texture
    void ReloadColorBuffer();

    /// Copies the 3DS depth framebuffer into the OpenGL depth framebuffer texture
    void ReloadDepthBuffer();

    /**
     * Save the current OpenGL color framebuffer to the current PICA framebuffer in 3DS memory
     * Loads the OpenGL framebuffer textures into temporary buffers
     * Then copies into the 3DS framebuffer using proper Morton order
     */
    void CommitColorBuffer();

    /**
     * Save the current OpenGL depth framebuffer to the current PICA framebuffer in 3DS memory
     * Loads the OpenGL framebuffer textures into temporary buffers
     * Then copies into the 3DS framebuffer using proper Morton order
     */
    void CommitDepthBuffer();

    RasterizerCacheOpenGL res_cache;

    std::vector<HardwareVertex> vertex_batch;

    OpenGLState state;

    PAddr last_fb_color_addr;
    PAddr last_fb_depth_addr;

    // Hardware rasterizer
    TextureInfo fb_color_texture;
    DepthTextureInfo fb_depth_texture;
    OGLShader shader;
    OGLVertexArray vertex_array;
    OGLBuffer vertex_buffer;
    OGLFramebuffer framebuffer;

    // Hardware vertex shader
    GLuint attrib_position;
    GLuint attrib_color;
    GLuint attrib_texcoords;

    // Hardware fragment shader
    GLuint uniform_alphatest_enabled;
    GLuint uniform_alphatest_func;
    GLuint uniform_alphatest_ref;
    GLuint uniform_tex;
    GLuint uniform_tev_combiner_buffer_color;
    TEVConfigUniforms uniform_tev_cfgs[6];
};
