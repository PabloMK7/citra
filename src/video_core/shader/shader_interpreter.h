// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Pica {

namespace Shader {

template <bool Debug>
struct UnitState;

template <bool Debug>
void RunInterpreter(const ShaderSetup& setup, UnitState<Debug>& state, unsigned offset);

} // namespace

} // namespace
