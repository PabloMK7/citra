#pragma once

#include <iterator>
#include <algorithm>

#include "video_core/pica.h"
#include "video_core/shader/shader.h"

namespace Pica {

class MemoryAccesses {
    /// Combine overlapping and close ranges
    void SimplifyRanges() {
        for (auto it = ranges.begin(); it != ranges.end(); ++it) {
            // NOTE: We add 32 to the range end address to make sure "close" ranges are combined, too
            auto it2 = std::next(it);
            while (it2 != ranges.end() && it->first + it->second + 32 >= it2->first) {
                it->second = std::max(it->second, it2->first + it2->second - it->first);
                it2 = ranges.erase(it2);
            }
        }
    }

public:
    /// Record a particular memory access in the list
    void AddAccess(u32 paddr, u32 size) {
        // Create new range or extend existing one
        ranges[paddr] = std::max(ranges[paddr], size);

        // Simplify ranges...
        SimplifyRanges();
    }

    /// Map of accessed ranges (mapping start address to range size)
    std::map<u32, u32> ranges;
};

class VertexLoader {
public:
    void Setup(const Pica::Regs& regs);
    void LoadVertex(int index, int vertex, Shader::InputVertex& input, MemoryAccesses& memory_accesses);

    u32 GetPhysicalBaseAddress() const { return base_address; }
    int GetNumTotalAttributes() const { return num_total_attributes; }
private:
    u32 vertex_attribute_sources[16];
    u32 vertex_attribute_strides[16] = {};
    Regs::VertexAttributeFormat vertex_attribute_formats[16] = {};
    u32 vertex_attribute_elements[16] = {};
    u32 vertex_attribute_element_size[16] = {};
    bool vertex_attribute_is_default[16];
    u32 base_address;
    int num_total_attributes;
};

}  // namespace Pica
