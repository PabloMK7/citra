/*!
* Copyright (C) 2005-2012 Gekko Emulator
*
* \file    timer.h
* \author  ShizZy <shizzy247@gmail.com>
* \date    2012-02-11
* \brief   Common time and timer routines
*
 * \section LICENSE
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

#include "SDL.h"

#include "common.h"
#include "timer.h"

namespace common {

/// Converts a ticks (miliseconds) u64 to a formatted string
void TicksToFormattedString(u32 ticks, char* formatted_string) {
    u32 hh = ticks / (1000 * 60 * 60);
    ticks -= hh * (1000 * 60 * 60);

    u32 mm = ticks / (1000 * 60);
    ticks -= mm * (1000 * 60);

    u32 ss = ticks / 1000;
    ticks -= ss * 1000;

    sprintf(formatted_string, "%02d:%02d:%03d", mm, ss, ticks);
}

} // namespace