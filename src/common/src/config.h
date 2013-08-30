/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    config.h
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-02-11
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

#ifndef COMMON_CONFIG_H_
#define COMMON_CONFIG_H_

#include "common.h"

#define MAX_SEARCH_PATHS    16  ///< Maximum paths to search for files in

/// If you need more than this... you're just lazy ;-)
#define MAX_PATCHES_PER_GAME    128 ///< Maximum patches allowed per game

namespace common {

/// Class for storing emulator configuration(s)
class Config {
public:
    Config();
    ~Config();

    /// Struct used for defining game-specific patches
    struct Patch {
        u32 address;    ///< Address to patch
        u32 data;       ///< Data to write at the specified address
    };

    /// Struct used for configuring what is inserted in a memory slot
    struct MemSlot {
        u8 device;      ///< Memory slot device (0 - memcard)
        bool enable;    ///< Enable (plugged in?)
    };

    enum Control {
        BUTTON_A = 0,
        BUTTON_B,
        BUTTON_X,
        BUTTON_Y,
        TRIGGER_L,
        TRIGGER_R,
        BUTTON_Z,
        BUTTON_START,
        ANALOG_UP,
        ANALOG_DOWN,
        ANALOG_LEFT,
        ANALOG_RIGHT,
        C_UP,
        C_DOWN,
        C_LEFT,
        C_RIGHT,
        DPAD_UP,
        DPAD_DOWN,
        DPAD_LEFT,
        DPAD_RIGHT,
        NUM_CONTROLS
    };

    /// Struct used for defining a keyboard configuration for a GameCube controller
    /// Reads/Writes from/to members should be atomic
    struct KeyboardController {
        bool enable;                ///< Is the keyboard configation enabled?
        int key_code[NUM_CONTROLS];
    };

    /// Struct used for defining a joypad configuration for a GameCube controller
    /// We'll make another struct in case the user wants seperate joypad config
    struct JoypadController {
        bool enable;                ///< Is the joypad configation enabled?
        int key_code[NUM_CONTROLS];
    };

    /// Struct used for configuring what is inserted in a controller port
    typedef struct {
        u8 device;                  ///< Controller port device (0 - controller)
        bool enable;                ///< Enable (plugged in?)
        KeyboardController keys;    ///< Keyboard configuration for controller (if used)
        JoypadController pads;      ///< Joypad configuration for controller (if used)
    } ControllerPort;

    /// Enum for supported CPU types
    enum CPUCoreType {
        CPU_NULL = 0,       ///< No CPU core
        CPU_INTERPRETER,    ///< Interpreter CPU core
        CPU_DYNAREC,        ///< Dynamic recompiler CPU core
        NUMBER_OF_CPU_CONFIGS
    };

    /// Struct used for defining a renderer configuration
    struct RendererConfig {
        bool enable_wireframe;
        bool enable_shaders;
        bool enable_texture_dumping;
        bool enable_textures;
        int anti_aliasing_mode;
        int anistropic_filtering_mode;
    } ;

    /// Struct used for configuring a screen resolution
    struct ResolutionType {
        int width;
        int height;
    };

    /// Enum for supported video cores
    enum RendererType {
        RENDERER_NULL,          ///< No video core
        RENDERER_OPENGL_2,      ///< OpenGL 2.0 core
        RENDERER_OPENGL_3,      ///< OpenGL 3.0 core (not implemented)
        RENDERER_DIRECTX9,      ///< DirectX9 core (not implemented)
        RENDERER_DIRECTX10,     ///< DirectX10 core (not implemented)
        RENDERER_DIRECTX11,     ///< DirectX11 core (not implemented)
        RENDERER_SOFTWARE,      ///< Software  core (not implemented)
        RENDERER_HARDWARE,      ///< Hardware core (not implemented- this would be a driver)
        NUMBER_OF_VIDEO_CONFIGS
    };
    
    char* program_dir() { return program_dir_; }
    void set_program_dir(const char* val, size_t size) { strcpy(program_dir_, val); }

    bool enable_multicore() { return enable_multicore_; }
    bool enable_idle_skipping() {return enable_idle_skipping_; }
    bool enable_hle() { return enable_hle_; }
    bool enable_auto_boot() { return enable_auto_boot_; }
    bool enable_cheats() { return enable_cheats_; }
    void set_enable_multicore(bool val) { enable_multicore_ = val; }
    void set_enable_idle_skipping(bool val) {enable_idle_skipping_ = val; }
    void set_enable_hle(bool val) { enable_hle_ = val; }
    void set_enable_auto_boot(bool val) { enable_auto_boot_ = val; }
    void set_enable_cheats(bool val) { enable_cheats_ = val; }

    char* default_boot_file() { return default_boot_file_; }
    char* dvd_image_path(int path) { return dvd_image_paths_[path]; }
    void set_default_boot_file(const char* val, size_t size) { strcpy(default_boot_file_, val); }
    void set_dvd_image_path(int path, char* val, size_t size) { strcpy(dvd_image_paths_[path], val); }

    bool enable_show_fps() { return enable_show_fps_; }
    bool enable_dump_opcode0() { return enable_dump_opcode0_; }
    bool enable_pause_on_unknown_opcode() { return enable_pause_on_unknown_opcode_; }
    bool enable_dump_gcm_reads() { return enable_dump_gcm_reads_; }
    void set_enable_show_fps(bool val) { enable_show_fps_ = val; }
    void set_enable_dump_opcode0(bool val) { enable_dump_opcode0_ = val; }
    void set_enable_pause_on_unknown_opcode(bool val) { enable_pause_on_unknown_opcode_ = val; }
    void set_enable_dump_gcm_reads(bool val) { enable_dump_gcm_reads_ = val; }

    bool enable_ipl() { return enable_ipl_; }
    void set_enable_ipl(bool val) { enable_ipl_ = val; }

    Patch patches(int patch) { return patches_[patch]; }
    Patch cheats(int cheat) { return cheats_[cheat]; }
    void set_patches(int patch, Patch val) { patches_[patch] = val; }
    void set_cheats(int cheat, Patch val) { cheats_[cheat] = val; }

    CPUCoreType powerpc_core() { return powerpc_core_; }
    void set_powerpc_core(CPUCoreType val) { powerpc_core_ = val; }

    int powerpc_frequency() { return powerpc_frequency_; }
    void set_powerpc_frequency(int val) { powerpc_frequency_ = val; }

    RendererType current_renderer() { return current_renderer_; }
    void set_current_renderer(RendererType val) { current_renderer_ = val; }

    RendererConfig renderer_config(RendererType val) { return renderer_config_[val]; }
    RendererConfig current_renderer_config() { return renderer_config_[current_renderer_]; }
    void set_renderer_config(RendererType renderer, RendererConfig config) { 
        renderer_config_[renderer] = config;
    }

    bool enable_fullscreen() { return enable_fullscreen_; }
    void set_enable_fullscreen(bool val) { enable_fullscreen_ = val; }

    ResolutionType window_resolution() { return window_resolution_; }
    ResolutionType fullscreen_resolution() { return fullscreen_resolution_; }
    void set_window_resolution(ResolutionType val) { window_resolution_ = val; }
    void set_fullscreen_resolution(ResolutionType val) { fullscreen_resolution_ = val; }

    // TODO: Should be const, but pending removal of some gekko_qt hacks
    /*const */ControllerPort& controller_ports(int port) { return controller_ports_[port]; }
    void set_controller_ports(int port, ControllerPort val) { controller_ports_[port] = val; }

    MemSlot mem_slots(int slot) { return mem_slots_[slot]; }
    void set_mem_slots(int slot, MemSlot val) { mem_slots_[slot] = val; }

    /**
     * @brief Gets a RenderType from a string (used from XML)
     * @param renderer_str Renderer name string, see XML schema for list
     * @return Corresponding RenderType
     */
    static inline RendererType StringToRenderType(const char* renderer_str) {
        if (E_OK == _stricmp(renderer_str, "opengl2")) {
            return RENDERER_OPENGL_2;
        } else if (E_OK == _stricmp(renderer_str, "opengl3")) {
            return RENDERER_OPENGL_3;
        } else if (E_OK == _stricmp(renderer_str, "directx9")) {
            return RENDERER_DIRECTX9;
        } else if (E_OK == _stricmp(renderer_str, "directx10")) {
            return RENDERER_DIRECTX10;
        } else if (E_OK == _stricmp(renderer_str, "directx11")) {
            return RENDERER_DIRECTX11;
        } else if (E_OK == _stricmp(renderer_str, "software")) {
            return RENDERER_SOFTWARE;
        } else if (E_OK == _stricmp(renderer_str, "hardware")) {
            return RENDERER_HARDWARE;
        } else {
            return RENDERER_NULL;
        }
    }

    /**
     * @brief Gets the renderer string from the type
     * @param renderer Renderer to get string for
     * @return Renderer string name
     */
    static std::string RenderTypeToString(RendererType renderer) {
        switch (renderer) {
        case RENDERER_OPENGL_2:
            return "opengl2";
        case RENDERER_OPENGL_3:
            return "opengl3";
        case RENDERER_DIRECTX9:
            return "directx9";
        case RENDERER_DIRECTX10:
            return "directx10";
        case RENDERER_DIRECTX11:
            return "directx11";
        case RENDERER_SOFTWARE:
            return "software";
        case RENDERER_HARDWARE:
            return "hardware";
        }
        return "null";
    }

    /**
     * @brief Gets the CPU string from the type
     * @param cpu CPU to get string for
     * @param cpu_str String result
     * @param size Max size to write to string
     */
    static std::string CPUCoreTypeToString(CPUCoreType cpu) {
        switch (cpu) {
        case CPU_INTERPRETER:
            return "interpreter";
        case CPU_DYNAREC:
            return "dynarec";
        }
        return "null";
    }

private:
    char program_dir_[MAX_PATH];

    bool enable_multicore_;
    bool enable_idle_skipping_;
    bool enable_hle_;
    bool enable_auto_boot_;
    bool enable_cheats_;

    char default_boot_file_[MAX_PATH];
    char dvd_image_paths_[MAX_SEARCH_PATHS][MAX_PATH];

    bool enable_show_fps_;
    bool enable_dump_opcode0_;
    bool enable_pause_on_unknown_opcode_;
    bool enable_dump_gcm_reads_;

    bool enable_ipl_;

    Patch patches_[MAX_PATCHES_PER_GAME];
    Patch cheats_[MAX_PATCHES_PER_GAME];

    CPUCoreType powerpc_core_;

    int powerpc_frequency_;

    bool enable_fullscreen_;

    RendererType current_renderer_;
    
    ResolutionType window_resolution_;
    ResolutionType fullscreen_resolution_;

    RendererConfig renderer_config_[NUMBER_OF_VIDEO_CONFIGS];

    MemSlot mem_slots_[2];
    ControllerPort controller_ports_[4];

    DISALLOW_COPY_AND_ASSIGN(Config);
};

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();

    /**
     * @brief Reload a game-specific configuration
     * @param id Game id (to load game specific configuration)
     */
    void ReloadGameConfig(const char* id);

    /// Reload the userconfig file
    void ReloadUserConfig();

    // Reload the sysconfig file
    void ReloadSysConfig();

    /// Reload all configurations
    void ReloadConfig(const char* game_id);

    char* program_dir() { return program_dir_; }

    void set_program_dir(const char* val, size_t size) { strcpy(program_dir_, val); }

private:
    char program_dir_[MAX_PATH]; ///< Program directory, used for loading config files

    DISALLOW_COPY_AND_ASSIGN(ConfigManager);
};

extern Config* g_config; ///< Global configuration for emulator

} // namspace

#endif // COMMON_CONFIG_H_
