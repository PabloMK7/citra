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

#ifndef COMMON_XML_H_
#define COMMON_XML_H_

#include "common.h"

namespace common {

/**
 * @brief Loads/parses an XML configuration file
 * @param config Reference to configuration object to populate
 * @param filename Filename of XMl file to load
 */
void LoadXMLConfig(Config& config, const char* filename);

} // namespace

#endif // COMMON_XML_H_
