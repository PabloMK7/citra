/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    config.cpp
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-02-19
 * @brief   Emulator configuration class - all config settings stored here
 *
 * @section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * Official project repository can be found at:
 * http://code.google.com/p/gekko-gc-emu/
 */

#include "common.h"
#include "config.h"
#include "xml.h"

namespace common {

Config* g_config;

Config::Config() {
    ResolutionType default_res;
    RendererConfig default_renderer_config;

    default_renderer_config.enable_wireframe = false;
    default_renderer_config.enable_shaders = true;
    default_renderer_config.enable_texture_dumping = false;
    default_renderer_config.enable_textures = true;
    default_renderer_config.anti_aliasing_mode = 0;
    default_renderer_config.anistropic_filtering_mode = 0;

    default_res.width = 640;
    default_res.height = 480;

    set_program_dir("", MAX_PATH);
    set_enable_multicore(true);
    set_enable_idle_skipping(false);
    set_enable_hle(true);
    set_enable_auto_boot(true);
    set_enable_cheats(false);
    set_default_boot_file("", MAX_PATH);
    memset(dvd_image_paths_, 0, sizeof(dvd_image_paths_));
    set_enable_show_fps(true);
    set_enable_dump_opcode0(false);
    set_enable_pause_on_unknown_opcode(true);
    set_enable_dump_gcm_reads(false);
    set_enable_ipl(false);
    set_powerpc_core(CPU_INTERPRETER);
    set_powerpc_frequency(486);
    
    memset(renderer_config_, 0, sizeof(renderer_config_));
    set_renderer_config(RENDERER_OPENGL_3, default_renderer_config);
    set_current_renderer(RENDERER_OPENGL_3);

    set_enable_fullscreen(false);
    set_window_resolution(default_res);
    set_fullscreen_resolution(default_res);

    memset(controller_ports_, 0, sizeof(controller_ports_));
    memset(mem_slots_, 0, sizeof(mem_slots_));

    memset(patches_, 0, sizeof(patches_));
    memset(cheats_, 0, sizeof(patches_));
}

Config::~Config() {
}

ConfigManager::ConfigManager() {
    set_program_dir("", MAX_PATH);
}

ConfigManager::~ConfigManager() {
}

/**
 * @brief Reload a game-specific configuration
 * @param id Game id (to load game specific configuration)
 */
void ConfigManager::ReloadGameConfig(const char* id) {
    char full_filename[MAX_PATH];
    sprintf(full_filename, "user/games/%s.xml", id);
    common::LoadXMLConfig(*g_config, full_filename);
}

/// Reload the userconfig file
void ConfigManager::ReloadUserConfig() {
    common::LoadXMLConfig(*g_config, "userconf.xml");
}

/// Reload the sysconfig file
void ConfigManager::ReloadSysConfig() {
    common::LoadXMLConfig(*g_config, "sysconf.xml");
}

/// Reload all configurations
void ConfigManager::ReloadConfig(const char* game_id) {
    delete g_config;
	g_config = new Config();
    g_config->set_program_dir(program_dir_, MAX_PATH);
    ReloadSysConfig();
    ReloadUserConfig();
    ReloadGameConfig(game_id);
}

} // namspace