// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "audio_core/audio_core.h"
#include "core/gdbstub/gdbstub.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/ir/ir.h"
#include "core/settings.h"
#include "video_core/video_core.h"

#include "core/frontend/emu_window.h"

namespace Settings {

Values values = {};

void Apply() {

    GDBStub::SetServerPort(values.gdbstub_port);
    GDBStub::ToggleServer(values.use_gdbstub);

    VideoCore::g_hw_renderer_enabled = values.use_hw_renderer;
    VideoCore::g_shader_jit_enabled = values.use_shader_jit;
    VideoCore::g_toggle_framelimit_enabled = values.toggle_framelimit;

    if (VideoCore::g_emu_window) {
        auto layout = VideoCore::g_emu_window->GetFramebufferLayout();
        VideoCore::g_emu_window->UpdateCurrentFramebufferLayout(layout.width, layout.height);
    }

    AudioCore::SelectSink(values.sink_id);
    AudioCore::EnableStretching(values.enable_audio_stretching);

    Service::HID::ReloadInputDevices();
    Service::IR::ReloadInputDevices();
}

} // namespace Settings
