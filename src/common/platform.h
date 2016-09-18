/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    platform.h
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-02-11
 * @brief   Platform detection macros for portable compilation
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

#pragma once

////////////////////////////////////////////////////////////////////////////////////////////////////
// Platform detection

#if defined(ARCHITECTURE_x86_64) || defined(__aarch64__)
#define EMU_ARCH_BITS 64
#elif defined(__i386) || defined(_M_IX86) || defined(__arm__) || defined(_M_ARM)
#define EMU_ARCH_BITS 32
#endif
