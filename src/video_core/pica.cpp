// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include "video_core/pica.h"
#include "video_core/pica_state.h"
#include "video_core/regs.h"

namespace Pica {

State g_state;

void Init() {
    g_state.Reset();
}

void Shutdown() {
    Shader::Shutdown();
}

template <typename T>
void Zero(T& o) {
    memset(&o, 0, sizeof(o));
}

void State::Reset() {
    Zero(regs);
    Zero(vs);
    Zero(gs);
    Zero(cmd_list);
    Zero(immediate);
    primitive_assembler.Reconfigure(PipelineRegs::TriangleTopology::List);
}
}
