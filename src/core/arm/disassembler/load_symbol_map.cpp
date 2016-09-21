// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <sstream>
#include <string>
#include <vector>
#include "common/file_util.h"
#include "common/symbols.h"
#include "core/arm/disassembler/load_symbol_map.h"

/*
 * Loads a symbol map file for use with the disassembler
 * @param filename String filename path of symbol map file
 */
void LoadSymbolMap(std::string filename) {
    std::ifstream infile(filename);

    std::string address_str, function_name, line;
    u32 size;

    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        if (!(iss >> address_str >> size >> function_name)) {
            break; // Error parsing
        }
        u32 address = std::stoul(address_str, nullptr, 16);

        Symbols::Add(address, function_name, size, 2);
    }
}
