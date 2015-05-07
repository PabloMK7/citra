// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

// Directory separators, do we need this?
#define DIR_SEP "/"
#define DIR_SEP_CHR '/'

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

// The user data dir
#define ROOT_DIR "."
#define USERDATA_DIR "user"
#ifdef USER_DIR
    #define EMU_DATA_DIR USER_DIR
#else
    #ifdef _WIN32
        #define EMU_DATA_DIR "Citra Emulator"
    #else
        #define EMU_DATA_DIR "citra-emu"
    #endif
#endif

// Dirs in both User and Sys
#define EUR_DIR "EUR"
#define USA_DIR "USA"
#define JAP_DIR "JAP"

// Subdirs in the User dir returned by GetUserPath(D_USER_IDX)
#define CONFIG_DIR               "config"
#define GAMECONFIG_DIR           "game_config"
#define MAPS_DIR                 "maps"
#define CACHE_DIR                "cache"
#define SDMC_DIR                 "sdmc"
#define NAND_DIR                 "nand"
#define SYSDATA_DIR              "sysdata"
#define SHADERCACHE_DIR          "shader_cache"
#define STATESAVES_DIR           "state_saves"
#define SCREENSHOTS_DIR          "screenShots"
#define DUMP_DIR                 "dump"
#define DUMP_TEXTURES_DIR        "textures"
#define DUMP_FRAMES_DIR          "frames"
#define DUMP_AUDIO_DIR           "audio"
#define LOGS_DIR                 "logs"
#define SHADERS_DIR              "shaders"
#define SYSCONF_DIR              "sysconf"

// Filenames
// Files in the directory returned by GetUserPath(D_CONFIG_IDX)
#define EMU_CONFIG        "emu.ini"
#define DEBUGGER_CONFIG   "debugger.ini"
#define LOGGER_CONFIG     "logger.ini"

// Sys files
#define SHARED_FONT       "shared_font.bin"

// Files in the directory returned by GetUserPath(D_LOGS_IDX)
#define MAIN_LOG "emu.log"

// Files in the directory returned by GetUserPath(D_SYSCONF_IDX)
#define SYSCONF "SYSCONF"
