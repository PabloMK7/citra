#pragma once

#include <array>

#include "common/common_types.h"
#include "video_core/pica.h"

namespace Pica {

namespace DebugUtils {
class MemoryAccessTracker;
}

namespace Shader {
struct InputVertex;
}

class VertexLoader {
public:
    VertexLoader() = default;
    explicit VertexLoader(const Pica::Regs& regs) {
        Setup(regs);
    }

    void Setup(const Pica::Regs& regs);
    void LoadVertex(u32 base_address, int index, int vertex, Shader::InputVertex& input, DebugUtils::MemoryAccessTracker& memory_accesses);

    int GetNumTotalAttributes() const { return num_total_attributes; }

private:
    std::array<u32, 16> vertex_attribute_sources;
    std::array<u32, 16> vertex_attribute_strides{};
    std::array<Regs::VertexAttributeFormat, 16> vertex_attribute_formats;
    std::array<u32, 16> vertex_attribute_elements{};
    std::array<bool, 16> vertex_attribute_is_default;
    int num_total_attributes = 0;
    bool is_setup = false;
};

}  // namespace Pica
