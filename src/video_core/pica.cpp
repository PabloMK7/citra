// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <unordered_map>

#include "pica.h"

namespace Pica {

State g_state;

std::string Regs::GetCommandName(int index) {
    static std::unordered_map<u32, std::string> map;

    if (map.empty()) {
        #define ADD_FIELD(name) \
                map.insert({static_cast<u32>(PICA_REG_INDEX(name)), #name}); \
                /* TODO: change to Regs::name when VS2015 and other compilers support it  */ \
                for (u32 i = PICA_REG_INDEX(name) + 1; i < PICA_REG_INDEX(name) + sizeof(Regs().name) / 4; ++i) \
                    map.insert({i, #name + std::string("+") + std::to_string(i-PICA_REG_INDEX(name))}); \

        ADD_FIELD(trigger_irq);
        ADD_FIELD(cull_mode);
        ADD_FIELD(viewport_size_x);
        ADD_FIELD(viewport_size_y);
        ADD_FIELD(viewport_depth_range);
        ADD_FIELD(viewport_depth_far_plane);
        ADD_FIELD(viewport_corner);
        ADD_FIELD(texture0_enable);
        ADD_FIELD(texture0);
        ADD_FIELD(texture0_format);
        ADD_FIELD(texture1);
        ADD_FIELD(texture1_format);
        ADD_FIELD(texture2);
        ADD_FIELD(texture2_format);
        ADD_FIELD(tev_stage0);
        ADD_FIELD(tev_stage1);
        ADD_FIELD(tev_stage2);
        ADD_FIELD(tev_stage3);
        ADD_FIELD(tev_combiner_buffer_input);
        ADD_FIELD(tev_stage4);
        ADD_FIELD(tev_stage5);
        ADD_FIELD(tev_combiner_buffer_color);
        ADD_FIELD(output_merger);
        ADD_FIELD(framebuffer);
        ADD_FIELD(vertex_attributes);
        ADD_FIELD(index_array);
        ADD_FIELD(num_vertices);
        ADD_FIELD(trigger_draw);
        ADD_FIELD(trigger_draw_indexed);
        ADD_FIELD(vs_default_attributes_setup);
        ADD_FIELD(command_buffer);
        ADD_FIELD(triangle_topology);
        ADD_FIELD(gs.bool_uniforms);
        ADD_FIELD(gs.int_uniforms);
        ADD_FIELD(gs.main_offset);
        ADD_FIELD(gs.input_register_map);
        ADD_FIELD(gs.uniform_setup);
        ADD_FIELD(gs.program);
        ADD_FIELD(gs.swizzle_patterns);
        ADD_FIELD(vs.bool_uniforms);
        ADD_FIELD(vs.int_uniforms);
        ADD_FIELD(vs.main_offset);
        ADD_FIELD(vs.input_register_map);
        ADD_FIELD(vs.uniform_setup);
        ADD_FIELD(vs.program);
        ADD_FIELD(vs.swizzle_patterns);

#undef ADD_FIELD
    }

    // Return empty string if no match is found
    auto it = map.find(index);
    if (it != map.end()) {
        return it->second;
    } else {
        return std::string();
    }
}

void Init() {
}

void Shutdown() {
    memset(&g_state, 0, sizeof(State));
}

}
