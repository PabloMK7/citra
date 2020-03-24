//? #version 320 es
#extension GL_ARM_shader_framebuffer_fetch_depth_stencil : enable

out highp uint color;

void main() {
    color = uint(gl_LastFragDepthARM * (exp2(24.0) - 1.0)) << 8;
    color |= uint(gl_LastFragStencilARM);
}
