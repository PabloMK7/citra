// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "video_core/rasterizer_interface.h"

namespace Frontend {
class EmuWindow;
}

namespace Pica {
struct Regs;
}

namespace Pica::Shader {
struct ShaderSetup;
}

namespace OpenGL {

class Driver;
class OpenGLState;

/// A class that manage different shader stages and configures them with given config data.
class ShaderProgramManager {
public:
    ShaderProgramManager(Frontend::EmuWindow& emu_window, const Driver& driver, bool separable);
    ~ShaderProgramManager();

    void LoadDiskCache(const std::atomic_bool& stop_loading,
                       const VideoCore::DiskResourceLoadCallback& callback);

    bool UseProgrammableVertexShader(const Pica::Regs& config, Pica::Shader::ShaderSetup& setup);

    void UseTrivialVertexShader();

    void UseFixedGeometryShader(const Pica::Regs& regs);

    void UseTrivialGeometryShader();

    void UseFragmentShader(const Pica::Regs& config);

    void ApplyTo(OpenGLState& state);

private:
    class Impl;
    std::unique_ptr<Impl> impl;

    Frontend::EmuWindow& emu_window;
    const Driver& driver;
};
} // namespace OpenGL
