/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    misc_utils.cpp
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-03-06
 * @brief   Miscellaneous functions/utilities that are used everywhere
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

#include "misc_utils.h"

namespace common {

/// Make a string lowercase
void LowerStr(char* str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower(str[ i ]);
    }
}

/// Make a string uppercase
void UpperStr(char* str) {
    for (int i=0; i < strlen(str); i++) {
        if(str[i] >= 'a' && str[i] <= 'z') {
            str[i] &= 0xDF;
        }
    }
}

/// Format a std::string using C-style sprintf formatting
std::string FormatStr(const char* format, ...) {
    va_list args;
    char *buf = NULL;
#if EMU_PLATFORM == PLATFORM_WINDOWS
    int required = 0;

    va_start(args, format);
    required = _vscprintf(format, args);
    buf = new char[required + 1];
    vsnprintf(buf, required, format, args);
    va_end(args);

    buf[required] = '\0';
    std::string temp = buf;
    delete[] buf;
#else
    va_start(args, format);
    vasprintf(&buf, format, args);
    va_end(args);

    std::string temp = buf;
    free(buf);
#endif
    return temp;
}

/// Check if a file exists
bool FileExists(char* filename) {
    std::ifstream ifile(filename);
    if (!ifile) {
        return false;
    }
    ifile.close();
    return true;
}

/// Gets the size of a file
size_t FileSize(FILE* file) {
    size_t pos = ftell(file);
    fseek(file, 0L, SEEK_END);
    size_t res = ftell(file);
    fseek(file, pos, SEEK_SET);
    return res;
}

} // namespace
