// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common.h"
#include "common/log_manager.h"
#include "common/file_util.h"

#include "core/system.h"
#include "core/core.h"
#include "core/loader/loader.h"

#include "citra/emu_window/emu_window_glfw.h"

#include "citra/citra.h"

/// Application entry point
int __cdecl main(int argc, char **argv) {
    LogManager::Init();

    if (argc < 2) {
        ERROR_LOG(BOOT, "Failed to load ROM: No ROM specified");
        return -1;
    }

    std::string boot_filename = argv[1];
    EmuWindow_GLFW* emu_window = new EmuWindow_GLFW;

    System::Init(emu_window);

    if (Loader::ResultStatus::Success != Loader::LoadFile(boot_filename)) {
        ERROR_LOG(BOOT, "Failed to load ROM!");
        return -1;
    }

    Core::RunLoop();

    delete emu_window;

    return 0;
}
