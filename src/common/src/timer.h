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

#ifndef COMMON_TIMER_H_
#define COMMON_TIMER_H_

#include "types.h"

namespace common {

/*!
 * \brief Gets Get the number of milliseconds since initialization
 * \return Unsigned integer of ticks since software initialization
 */
static inline u32 GetTimeElapsed() {
    return SDL_GetTicks();
}

/*!
 * \brief Converts a ticks (miliseconds) u32 to a formatted string
 * \param ticks Ticks (32-bit unsigned integer)
 * \param formatted_string Pointer to formatted string result
 */
void TicksToFormattedString(u32 ticks, char* formatted_string);


} // namespace


#endif // COMMON_TIMER_H_