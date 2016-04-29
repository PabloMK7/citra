#pragma once

#include <iterator>
#include <algorithm>

#include "video_core/pica.h"
#include "video_core/shader/shader.h"
#include "video_core/debug_utils/debug_utils.h"

namespace Pica {

class VertexLoader {
public:
    void Setup(const Pica::Regs& regs);
    void LoadVertex(u32 base_address, int index, int vertex, Shader::InputVertex& input, DebugUtils::MemoryAccessTracker& memory_accesses);

    int GetNumTotalAttributes() const { return num_total_attributes; }

private:
    u32 vertex_attribute_sources[16];
    u32 vertex_attribute_strides[16] = {};
    Regs::VertexAttributeFormat vertex_attribute_formats[16] = {};
    u32 vertex_attribute_elements[16] = {};
    bool vertex_attribute_is_default[16];
    int num_total_attributes;
};

}  // namespace Pica
