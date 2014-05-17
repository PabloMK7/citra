// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common.h"
#include "common/log_manager.h"
#include "common/file_util.h"

#include "core/system.h"
#include "core/core.h"
#include "core/loader.h"

#include "citra/emu_window/emu_window_glfw.h"

#include "citra/citra.h"

/// Application entry point
int __cdecl main(int argc, char **argv) {
    std::string program_dir = File::GetCurrentDir();

    LogManager::Init();

    EmuWindow_GLFW* emu_window = new EmuWindow_GLFW;

    System::Init(emu_window);

    std::string boot_filename = "homebrew.elf";
    std::string error_str;

    bool res = Loader::LoadFile(boot_filename, &error_str);

    if (!res) {
        ERROR_LOG(BOOT, "Failed to load ROM: %s", error_str.c_str());
    }

    Core::RunLoop();

    delete emu_window;

    return 0;
}
