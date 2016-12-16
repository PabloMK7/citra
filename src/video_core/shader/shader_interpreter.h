// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Pica {

namespace Shader {

struct UnitState;

template <bool Debug>
struct DebugData;

template <bool Debug>
void RunInterpreter(const ShaderSetup& setup, UnitState& state, DebugData<Debug>& debug_data,
                    unsigned offset);

} // namespace

} // namespace
