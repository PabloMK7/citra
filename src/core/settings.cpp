// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "settings.h"

#include "audio_core/audio_core.h"

#include "core/gdbstub/gdbstub.h"

#include "video_core/video_core.h"

namespace Settings {

Values values = {};

void Apply() {

    GDBStub::SetServerPort(static_cast<u32>(values.gdbstub_port));
    GDBStub::ToggleServer(values.use_gdbstub);

    VideoCore::g_hw_renderer_enabled = values.use_hw_renderer;
    VideoCore::g_shader_jit_enabled = values.use_shader_jit;
    VideoCore::g_scaled_resolution_enabled = values.use_scaled_resolution;

    AudioCore::SelectSink(values.sink_id);
    AudioCore::EnableStretching(values.enable_audio_stretching);

}

} // namespace
