// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common.h"
#include "common/log_manager.h"

#include "core/system.h"
#include "core/core.h"
#include "core/loader/loader.h"

#include "citra/config.h"
#include "citra/emu_window/emu_window_glfw.h"

/// Application entry point
int __cdecl main(int argc, char **argv) {
    LogManager::Init();

    if (argc < 2) {
        ERROR_LOG(BOOT, "Failed to load ROM: No ROM specified");
        return -1;
    }

    Config config;

    std::string boot_filename = argv[1];
    EmuWindow_GLFW* emu_window = new EmuWindow_GLFW;

    System::Init(emu_window);

    Loader::ResultStatus load_result = Loader::LoadFile(boot_filename);
    if (Loader::ResultStatus::Success != load_result) {
        ERROR_LOG(BOOT, "Failed to load ROM (Error %i)!", load_result);
        return -1;
    }

    while(true) {
        Core::RunLoop();
    }

    delete emu_window;

    return 0;
}
