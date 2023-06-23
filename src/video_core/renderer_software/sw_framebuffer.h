// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "common/vector_math.h"
#include "video_core/regs_framebuffer.h"

namespace Memory {
class MemorySystem;
}

namespace Pica {
struct FramebufferRegs;
}

namespace SwRenderer {

class Framebuffer {
public:
    explicit Framebuffer(Memory::MemorySystem& memory, const Pica::FramebufferRegs& framebuffer);
    ~Framebuffer();

    /// Draws a pixel at the specified coordinates.
    void DrawPixel(int x, int y, const Common::Vec4<u8>& color) const;

    /// Returns the current color at the specified coordinates.
    [[nodiscard]] const Common::Vec4<u8> GetPixel(int x, int y) const;

    /// Returns the depth value at the specified coordinates.
    [[nodiscard]] u32 GetDepth(int x, int y) const;

    /// Returns the stencil value at the specified coordinates.
    [[nodiscard]] u8 GetStencil(int x, int y) const;

    /// Stores the provided depth value at the specified coordinates.
    void SetDepth(int x, int y, u32 value) const;

    /// Stores the provided stencil value at the specified coordinates.
    void SetStencil(int x, int y, u8 value) const;

    /// Draws a pixel to the shadow buffer.
    void DrawShadowMapPixel(int x, int y, u32 depth, u8 stencil) const;

private:
    Memory::MemorySystem& memory;
    const Pica::FramebufferRegs& regs;
};

u8 PerformStencilAction(Pica::FramebufferRegs::StencilAction action, u8 old_stencil, u8 ref);

Common::Vec4<u8> EvaluateBlendEquation(const Common::Vec4<u8>& src,
                                       const Common::Vec4<u8>& srcfactor,
                                       const Common::Vec4<u8>& dest,
                                       const Common::Vec4<u8>& destfactor,
                                       Pica::FramebufferRegs::BlendEquation equation);

u8 LogicOp(u8 src, u8 dest, Pica::FramebufferRegs::LogicOp op);

} // namespace SwRenderer
