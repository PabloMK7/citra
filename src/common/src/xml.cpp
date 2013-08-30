/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    xml.h
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-02-12
 * @brief   Used for parsing XML configurations
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

#include <iostream>
#include <fstream>

#include <rapidxml.hpp>

#include "common.h"
#include "misc_utils.h"
#include "config.h"
#include "log.h"

/// Gets a RapidXML boolean element value
static bool GetXMLElementAsBool(rapidxml::xml_node<> *node, const char* element_name) {
    rapidxml::xml_node<> *sub_node = node->first_node(element_name);
    if (sub_node) {
        return (E_OK == _stricmp(sub_node->value(), "true")) ? true : false;
    }
    return false;
}

/// Gets a RapidXML string element value
static char* GetXMLElementAsString(rapidxml::xml_node<> *node, const char* element_name, 
    char* element_value) {
    rapidxml::xml_node<> *sub_node = node->first_node(element_name);
    if (sub_node) {
        strcpy(element_value, sub_node->value());
        return element_value;
    }
    return NULL;
}

/// Gets a RapidXML integer element value
static int GetXMLElementAsInt(rapidxml::xml_node<> *node, const char* element_name) {
    rapidxml::xml_node<> *sub_node = node->first_node(element_name);
    if (sub_node) {
        return atoi(sub_node->value());
    }
    return 0;
}

namespace common {

/**
 * @brief Parse the "General" XML group
 * @param node RapidXML node for the "General" XML group
 * @param config Config class object to parse data into
 */
void ParseGeneralNode(rapidxml::xml_node<> *node, Config& config) {
    // Don't parse the node if it doesn't exist!
    if (!node) {
        return;
    }
    char temp_str[MAX_PATH];
    config.set_enable_multicore(GetXMLElementAsBool(node, "EnableMultiCore"));
    config.set_enable_idle_skipping(GetXMLElementAsBool(node, "EnableIdleSkipping"));
    config.set_enable_hle(GetXMLElementAsBool(node, "EnableHLE"));
    config.set_enable_auto_boot(GetXMLElementAsBool(node, "EnableAutoBoot"));
    config.set_enable_cheats(GetXMLElementAsBool(node, "EnableCheats"));
    config.set_default_boot_file(GetXMLElementAsString(node, "DefaultBootFile", temp_str), MAX_PATH);

    // Parse all search paths in the DVDImagePaths node
    rapidxml::xml_node<> *sub_node = node->first_node("DVDImagePaths");
    if (sub_node) {
        int i = 0;
        for (rapidxml::xml_node<> *elem = sub_node->first_node("Path"); elem; 
            elem = elem->next_sibling()) {
            
            config.set_dvd_image_path(i, elem->value(), MAX_PATH);
            LOG_NOTICE(TCONFIG, "Adding %s to DVD image search paths...\n", 
                config.dvd_image_path(i));
            i++;
            // Stop if we have parsed the maximum paths
            if (MAX_SEARCH_PATHS < i) {
                LOG_WARNING(TCONFIG, "Maximum number of DVDImagePath search paths is %d, not parsing"
                    " any more!", MAX_SEARCH_PATHS);
                break;
            }
        }
    }
}

/**
 * @brief Parse the "Debug" XML group
 * @param node RapidXML node for the "Debug" XML group
 * @param config Config class object to parse data into
 */
void ParseDebugNode(rapidxml::xml_node<> *node, Config& config) {
    // Don't parse the node if it doesn't exist!
    if (!node) {
        return;
    }
    config.set_enable_show_fps(GetXMLElementAsBool(node, "EnableShowFPS"));
    config.set_enable_dump_opcode0(GetXMLElementAsBool(node, "EnableDumpOpcode0"));
    config.set_enable_pause_on_unknown_opcode(GetXMLElementAsBool(node, 
        "EnablePauseOnUnknownOpcode"));
    config.set_enable_dump_gcm_reads(GetXMLElementAsBool(node, "EnableDumpGCMReads"));
}

/**
 * @brief Parse the "Patches" and "Cheats" XML group
 * @param node RapidXML node for the "Patches" or "Cheats" XML group
 * @param config Config class object to parse data into
 */
void ParsePatchesNode(rapidxml::xml_node<> *node, Config& config, const char* node_name) {
    int i = 0;
    char node_name_str[8];
    
    // Get lowercase section name
    strcpy(node_name_str, node_name);

    // TODO: not available on Unix
    common::LowerStr(node_name_str);
    
    // Parse all search patches in the Patches node
    rapidxml::xml_node<> *sub_node = node->first_node(node_name);
    if (sub_node) {
        for (rapidxml::xml_node<> *elem = sub_node->first_node("Patch"); elem; 
            elem = elem->next_sibling()) {

            // Get enable attribute (note: defaults to true)
            rapidxml::xml_attribute<> *attr = elem->first_attribute("enable");
            if (attr) {
                if (E_OK == _stricmp(attr->value(), "false")) {
                    continue; // Patch is disabled, skip it
                }
            }
            // Get address attribute
            attr = elem->first_attribute("address");
            if (!attr) {
                LOG_ERROR(TCONFIG, "Patch without 'address' attribute illegal!");
                continue;
            } else {
                u32 data = 0;
                u32 address = 0;
                {
                    // Convert address hexstring to unsigned int
                    std::stringstream ss;
                    ss << std::hex << attr->value();
                    ss >> address;
                }
                attr = elem->first_attribute("instr");

                // Get "data" attribute if no "instr" attribute
                if (!attr) {
                    attr = elem->first_attribute("data");
                    // Neither found - error
                    if (!attr) {
                        LOG_ERROR(TCONFIG, "Patch without 'instr' or 'data' attributes "
                            "illegal!");
                        continue;
                    } else {
                        // Found data, convert hexstring to unsigned int
                        std::stringstream ss;
                        ss << std::hex << attr->value();
                        ss >> data;
                    }
                } else {
                    // Found instr
                    char instr_str[4];
                    
                    // Convert to lowercase
                    strcpy(instr_str, attr->value());
                    // TODO: not available on Unix
                    common::LowerStr(instr_str);

                    // Convert instruction to equivalent PPC bytecode
                    //  TODO(ShizZy): Pull this out to the PowerPC modules at some point
                    if (E_OK == _stricmp(instr_str, "blr")) {
                        data = 0x4E800020; // PowerPC BLR instruction bytecode
                    } else if (E_OK == _stricmp(instr_str, "nop")) {
                        data = 0x60000000; // PowerPC NOP instruction bytecode
                    } else {
                        LOG_ERROR(TCONFIG, "Patch with invalid 'instr' attribute illegal!");
                        continue;
                    }
                }
                Config::Patch patch = { address, data };

                if (E_OK == _stricmp(node_name_str, "patches")) {
                    LOG_NOTICE(TCONFIG, "Adding patch addr=0x%08x data=0x%08x to patches...\n",
                        address, data, node_name_str);
                    config.set_patches(i, patch);
                } else if (E_OK == _stricmp(node_name_str, "cheats")) {
                    LOG_NOTICE(TCONFIG, "Adding cheat addr=0x%08x data=0x%08x to cheats...\n",
                        address, data, node_name_str);
                    config.set_cheats(i, patch);
                } else {
                    LOG_ERROR(TCONFIG, "Unexpected patch type %s, ignoring...", node_name_str);
                }

                // Stop if we have parsed the maximum patches
                if (MAX_PATCHES_PER_GAME < ++i) {
                    LOG_WARNING(TCONFIG, "Maximum number of patches search paths is %d, not parsing"
                        " any more!", MAX_PATCHES_PER_GAME);
                    break;
                }
            }
        }
    }
}

/**
 * @brief Parse the "Boot" XML group
 * @param node RapidXML node for the "Boot" XML group
 * @param config Config class object to parse data into
 */
void ParseBootNode(rapidxml::xml_node<> *node, Config& config) {
    // Don't parse the node if it doesn't exist!
    if (!node) {
        return;
    }
    config.set_enable_ipl(GetXMLElementAsBool(node, "EnableIPL"));
    
    ParsePatchesNode(node, config, "Patches");
    ParsePatchesNode(node, config, "Cheats");
}

/**
 * @brief Parse the "Video" XML group
 * @param node RapidXML node for the "Video" XML group
 * @param config Config class object to parse data into
 */
void ParsePowerPCNode(rapidxml::xml_node<> *node, Config& config) {
    // Don't parse the node if it doesn't exist!
    if (!node) {
        return;
    }
    rapidxml::xml_attribute<> *attr = node->first_attribute("core");

    // Attribute not found - error
    if (!attr) {
        LOG_ERROR(TCONFIG, "PowerPC without 'core' attribute illegal!");
    } else {
        char core_str[12] = "null";
        
        // Convert to lowercase
        strcpy(core_str, attr->value());
        // TODO: not available on Unix
        common::LowerStr(core_str);

        // Use interpreter core
        if (E_OK == _stricmp(core_str, "interpreter")) {
            config.set_powerpc_core(Config::CPU_INTERPRETER);   // Interpreter selected
        // Use dynarec core
        } else if (E_OK == _stricmp(core_str, "dynarec")) {
            config.set_powerpc_core(Config::CPU_DYNAREC);       // Dynarec selected
        // Unsupported type
        } else {
            LOG_ERROR(TCONFIG, "Invalid PowerPC type %s for attribute 'core' selected!", 
                core_str);
        }
        // Set frequency
        attr = node->first_attribute("freq");
        if (attr) {
            config.set_powerpc_frequency(atoi(attr->value()));
        }
        LOG_NOTICE(TCONFIG, "Configured core=%s freq=%d", core_str, config.powerpc_frequency());
    }
}

/**
 * @brief Parse the "Video" XML group
 * @param node RapidXML node for the "Video" XML group
 * @param config Config class object to parse data into
 */
void ParseVideoNode(rapidxml::xml_node<> *node, Config& config) {
    char res_str[512];
    Config::ResolutionType res;

    // Don't parse the node if it doesn't exist!
    if (!node) {
        return;
    }
    config.set_enable_fullscreen(GetXMLElementAsBool(node, "EnableFullscreen"));
    
    // Set resolutions
    GetXMLElementAsString(node, "WindowResolution", res_str);
    sscanf(res_str, "%d_%d", &res.width, &res.height);
    config.set_window_resolution(res);
    GetXMLElementAsString(node, "FullscreenResolution", res_str);
    sscanf(res_str, "%d_%d", &res.width, &res.height);
    config.set_fullscreen_resolution(res);

    // Parse all search renderer nodes
    for (rapidxml::xml_node<> *elem = node->first_node("Renderer"); 1; ) {
        Config::RendererConfig renderer_config;

        rapidxml::xml_attribute<> *attr = elem->first_attribute("name");

        Config::RendererType type = Config::StringToRenderType(attr->value());
        
        renderer_config.enable_wireframe = GetXMLElementAsBool(elem, "EnableWireframe");
        renderer_config.enable_shaders = GetXMLElementAsBool(elem, "EnableShaders");
        renderer_config.enable_textures = GetXMLElementAsBool(elem, "EnableTextures");
        renderer_config.enable_texture_dumping = GetXMLElementAsBool(elem, "EnableTextureDumping");
        renderer_config.anti_aliasing_mode = GetXMLElementAsInt(elem, "AntiAliasingMode");
        renderer_config.anistropic_filtering_mode = GetXMLElementAsInt(elem, "AnistropicFilteringMode");

        config.set_renderer_config(type, renderer_config);

        LOG_NOTICE(TCONFIG, "Renderer %s configured", attr->value());

        break;
    }
}

/**
 * @brief Parse the "Devices" XML group
 * @param node RapidXML node for the "Devices" XML group
 * @param config Config class object to parse data into
 */
void ParseDevicesNode(rapidxml::xml_node<> *node, Config& config) {
    // Don't parse the node if it doesn't exist!c
    if (!node) {
        return;
    }
    // Parse GameCube section
    rapidxml::xml_node<> *gamecube_node = node->first_node("GameCube");
    if (!gamecube_node) {
        return;
    }
    // Parse all MemSlot nodes
    for (rapidxml::xml_node<> *elem = gamecube_node->first_node("MemSlot"); elem; 
        elem = elem->next_sibling("MemSlot")) {
        Config::MemSlot slot_config;

        // Select MemSlot a or b
        rapidxml::xml_attribute<> *attr = elem->first_attribute("slot");
        int slot = (E_OK == _stricmp(attr->value(), "a")) ? 0 : 1;
        
        // Enable
        attr = elem->first_attribute("enable");
        slot_config.enable = (E_OK == _stricmp(attr->value(), "true")) ? true : false;

        // Select device
        attr = elem->first_attribute("device");
        slot_config.device = 0; // Only support memcards right now

        LOG_NOTICE(TCONFIG, "Configured MemSlot[%d]=%s enabled=%s", slot, attr->value(), 
            slot_config.enable ? "true" : "false");

        config.set_mem_slots(slot, slot_config);
    }
    // Parse all ControlerPort nodes
    for (rapidxml::xml_node<> *elem = gamecube_node->first_node("ControllerPort"); elem; 
            elem = elem->next_sibling("ControllerPort")) {
        Config::ControllerPort port_config;

        // Select MemSlot a or b
        rapidxml::xml_attribute<> *attr = elem->first_attribute("port");
        int port = atoi(attr->value());

        // Enable
        attr = elem->first_attribute("enable");
        port_config.enable = (E_OK == _stricmp(attr->value(), "true")) ? true : false;
        
        // Select device
        attr = elem->first_attribute("device");
        port_config.device = 0; // Only support memcards right now

        LOG_NOTICE(TCONFIG, "Configured ControllerPort[%d]=%s enabled=%s", port, attr->value(),
            port_config.enable ? "true" : "false");

        // Parse keyboard configuration - TODO: Move to EmuWindow (?)
        rapidxml::xml_node<> *keyboard_node = elem->first_node("KeyboardController");
        if (keyboard_node) {
            attr = keyboard_node->first_attribute("enable");
            port_config.keys.enable = (E_OK == _stricmp(attr->value(), "true")) ? true : false;
            port_config.keys.key_code[Config::BUTTON_A] = GetXMLElementAsInt(keyboard_node, "AKey");
            port_config.keys.key_code[Config::BUTTON_B] = GetXMLElementAsInt(keyboard_node, "BKey");
            port_config.keys.key_code[Config::BUTTON_X] = GetXMLElementAsInt(keyboard_node, "XKey");
            port_config.keys.key_code[Config::BUTTON_Y] = GetXMLElementAsInt(keyboard_node, "YKey");
            port_config.keys.key_code[Config::TRIGGER_L] = GetXMLElementAsInt(keyboard_node, "LKey");
            port_config.keys.key_code[Config::TRIGGER_R] = GetXMLElementAsInt(keyboard_node, "RKey");
            port_config.keys.key_code[Config::BUTTON_Z] = GetXMLElementAsInt(keyboard_node, "ZKey");
            port_config.keys.key_code[Config::BUTTON_START] = GetXMLElementAsInt(keyboard_node, "StartKey");
            port_config.keys.key_code[Config::ANALOG_UP] = GetXMLElementAsInt(keyboard_node, "AnalogUpKey");
            port_config.keys.key_code[Config::ANALOG_DOWN] = GetXMLElementAsInt(keyboard_node, "AnalogDownKey");
            port_config.keys.key_code[Config::ANALOG_LEFT] = GetXMLElementAsInt(keyboard_node, "AnalogLeftKey");
            port_config.keys.key_code[Config::ANALOG_RIGHT] = GetXMLElementAsInt(keyboard_node, "AnalogRightKey");
            port_config.keys.key_code[Config::C_UP] = GetXMLElementAsInt(keyboard_node, "CUpKey");
            port_config.keys.key_code[Config::C_DOWN] = GetXMLElementAsInt(keyboard_node, "CDownKey");
            port_config.keys.key_code[Config::C_LEFT] = GetXMLElementAsInt(keyboard_node, "CLeftKey");
            port_config.keys.key_code[Config::C_RIGHT] = GetXMLElementAsInt(keyboard_node, "CRightKey");
            port_config.keys.key_code[Config::DPAD_UP] = GetXMLElementAsInt(keyboard_node, "DPadUpKey");
            port_config.keys.key_code[Config::DPAD_DOWN] = GetXMLElementAsInt(keyboard_node, "DPadDownKey");
            port_config.keys.key_code[Config::DPAD_LEFT] = GetXMLElementAsInt(keyboard_node, "DPadLeftKey");
            port_config.keys.key_code[Config::DPAD_RIGHT] = GetXMLElementAsInt(keyboard_node, "DPadRightKey");
        }

        // Parse joypad configuration
        rapidxml::xml_node<> *joypad_node = elem->first_node("JoypadController");
        if (joypad_node) {
            attr = joypad_node->first_attribute("enable");
            port_config.pads.enable = (E_OK == _stricmp(attr->value(), "true")) ? true : false;
            port_config.pads.key_code[Config::BUTTON_A] = GetXMLElementAsInt(joypad_node, "AKey");
            port_config.pads.key_code[Config::BUTTON_B] = GetXMLElementAsInt(joypad_node, "BKey");
            port_config.pads.key_code[Config::BUTTON_X] = GetXMLElementAsInt(joypad_node, "XKey");
            port_config.pads.key_code[Config::BUTTON_Y] = GetXMLElementAsInt(joypad_node, "YKey");
            port_config.pads.key_code[Config::TRIGGER_L] = GetXMLElementAsInt(joypad_node, "LKey");
            port_config.pads.key_code[Config::TRIGGER_R] = GetXMLElementAsInt(joypad_node, "RKey");
            port_config.pads.key_code[Config::BUTTON_Z] = GetXMLElementAsInt(joypad_node, "ZKey");
            port_config.pads.key_code[Config::BUTTON_START] = GetXMLElementAsInt(joypad_node, "StartKey");
            port_config.pads.key_code[Config::ANALOG_UP] = GetXMLElementAsInt(joypad_node, "AnalogUpKey");
            port_config.pads.key_code[Config::ANALOG_DOWN] = GetXMLElementAsInt(joypad_node, "AnalogDownKey");
            port_config.pads.key_code[Config::ANALOG_LEFT] = GetXMLElementAsInt(joypad_node, "AnalogLeftKey");
            port_config.pads.key_code[Config::ANALOG_RIGHT] = GetXMLElementAsInt(joypad_node, "AnalogRightKey");
            port_config.pads.key_code[Config::C_UP] = GetXMLElementAsInt(joypad_node, "CUpKey");
            port_config.pads.key_code[Config::C_DOWN] = GetXMLElementAsInt(joypad_node, "CDownKey");
            port_config.pads.key_code[Config::C_LEFT] = GetXMLElementAsInt(joypad_node, "CLeftKey");
            port_config.pads.key_code[Config::C_RIGHT] = GetXMLElementAsInt(joypad_node, "CRightKey");
            port_config.pads.key_code[Config::DPAD_UP] = GetXMLElementAsInt(joypad_node, "DPadUpKey");
            port_config.pads.key_code[Config::DPAD_DOWN] = GetXMLElementAsInt(joypad_node, "DPadDownKey");
            port_config.pads.key_code[Config::DPAD_LEFT] = GetXMLElementAsInt(joypad_node, "DPadLeftKey");
            port_config.pads.key_code[Config::DPAD_RIGHT] = GetXMLElementAsInt(joypad_node, "DPadRightKey");
        }
        config.set_controller_ports(port, port_config);
    }
}

/// Loads/parses an XML configuration file
void LoadXMLConfig(Config& config, const char* filename) {
    // Open the XML file
    char full_filename[MAX_PATH];
    strcpy(full_filename, config.program_dir());
    strcat(full_filename, filename);
    std::ifstream ifs(full_filename);

    // Check that the file is valid
    if (ifs.fail()) {
        LOG_ERROR(TCONFIG, "XML configuration file %s failed to open!", filename);
        return;
    }
    // Read and parse XML string
    std::string xml_str((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    rapidxml::xml_document<> doc;
    doc.parse<0>(const_cast<char *>(xml_str.c_str()));  

    // Try to load a system configuration
    rapidxml::xml_node<> *node = doc.first_node("SysConfig");

    // Try to load a game configuation
    if (!node) {
        node = doc.first_node("GameConfig");
    }
    // Try to load a user configuation
    if (!node) {
        node = doc.first_node("UserConfig");
    }
    // Not proper XML format
    if (!node) {
        LOG_ERROR(TCONFIG, "XML configuration file incorrect format %s!", filename)
        return;
    }
    // Parse all sub nodes into the config
    ParseGeneralNode(node->first_node("General"),   config);
    ParseDebugNode(node->first_node("Debug"),       config);
    ParseBootNode(node->first_node("Boot"),         config);
    ParsePowerPCNode(node->first_node("PowerPC"),   config);
    ParseVideoNode(node->first_node("Video"),       config);
    ParseDevicesNode(node->first_node("Devices"),   config);
}

} // namespace
