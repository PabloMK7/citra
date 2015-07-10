// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Pica {
namespace VertexShader {
struct OutputVertex;
}
}

class HWRasterizer {
public:
    virtual ~HWRasterizer() {
    }

    /// Initialize API-specific GPU objects
    virtual void InitObjects() = 0;

    /// Reset the rasterizer, such as flushing all caches and updating all state
    virtual void Reset() = 0;

    /// Queues the primitive formed by the given vertices for rendering
    virtual void AddTriangle(const Pica::VertexShader::OutputVertex& v0,
                             const Pica::VertexShader::OutputVertex& v1,
                             const Pica::VertexShader::OutputVertex& v2) = 0;

    /// Draw the current batch of triangles
    virtual void DrawTriangles() = 0;

    /// Commit the rasterizer's framebuffer contents immediately to the current 3DS memory framebuffer
    virtual void CommitFramebuffer() = 0;

    /// Notify rasterizer that the specified PICA register has been changed
    virtual void NotifyPicaRegisterChanged(u32 id) = 0;

    /// Notify rasterizer that the specified 3DS memory region will be read from after this notification
    virtual void NotifyPreRead(PAddr addr, u32 size) = 0;

    /// Notify rasterizer that a 3DS memory region has been changed
    virtual void NotifyFlush(PAddr addr, u32 size) = 0;
};
