/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    misc_utils.h
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

#include "common.h"

namespace common {

/**
 * @brief Make a string lowercase
 * @param str String to make lowercase
 */
void LowerStr(char* str);

/**
 * @brief Make a string uppercase
 * @param str String to make uppercase
 */
void UpperStr(char* str);

/// Format a std::string using C-style sprintf formatting
std::string FormatStr(const char* format, ...);

/**
 * @brief Check if a file exists on the users computer
 * @param filename Filename of file to check for
 * @return true on exists, false otherwise
 */
bool FileExists(char* filename);

/**
 * @brief Gets the size of a file
 * @param file Pointer to file to get size of
 * @return true Size of file, in bytes
 */
size_t FileSize(FILE* file);

} // namespace
