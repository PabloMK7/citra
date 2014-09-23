// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

// Make sure we pick up USER_DIR if set in config.h
#include "common/common.h"

// Directory seperators, do we need this?
#define DIR_SEP "/"
#define DIR_SEP_CHR '/'

#ifndef MAX_PATH
#define MAX_PATH    260
#endif

// The user data dir
#define ROOT_DIR "."
#ifdef _WIN32
    #define USERDATA_DIR "user"
    #define EMU_DATA_DIR "emu"
#else
    #define USERDATA_DIR "user"
    #ifdef USER_DIR
        #define EMU_DATA_DIR USER_DIR
    #else
        #define EMU_DATA_DIR ".emu"
    #endif
#endif

// Shared data dirs (Sys and shared User for linux)
#ifdef _WIN32
    #define SYSDATA_DIR "sys"
#else
    #ifdef DATA_DIR
        #define SYSDATA_DIR DATA_DIR "sys"
        #define SHARED_USER_DIR  DATA_DIR USERDATA_DIR DIR_SEP
    #else
        #define SYSDATA_DIR "sys"
        #define SHARED_USER_DIR  ROOT_DIR DIR_SEP USERDATA_DIR DIR_SEP
    #endif
#endif

// Dirs in both User and Sys
#define EUR_DIR "EUR"
#define USA_DIR "USA"
#define JAP_DIR "JAP"

// Subdirs in the User dir returned by GetUserPath(D_USER_IDX)
#define CONFIG_DIR            "config"
#define GAMECONFIG_DIR        "game_config"
#define MAPS_DIR            "maps"
#define CACHE_DIR            "cache"
#define SDMC_DIR          "sdmc"
#define SHADERCACHE_DIR        "shader_cache"
#define STATESAVES_DIR        "state_saves"
#define SCREENSHOTS_DIR        "screenShots"
#define DUMP_DIR            "dump"
#define DUMP_TEXTURES_DIR    "textures"
#define DUMP_FRAMES_DIR        "frames"
#define DUMP_AUDIO_DIR        "audio"
#define LOGS_DIR            "logs"
#define SHADERS_DIR         "shaders"
#define SYSCONF_DIR         "sysconf"

// Filenames
// Files in the directory returned by GetUserPath(D_CONFIG_IDX)
#define EMU_CONFIG        "emu.ini"
#define DEBUGGER_CONFIG    "debugger.ini"
#define LOGGER_CONFIG    "logger.ini"

// Files in the directory returned by GetUserPath(D_LOGS_IDX)
#define MAIN_LOG    "emu.log"

// Files in the directory returned by GetUserPath(D_SYSCONF_IDX)
#define SYSCONF    "SYSCONF"
