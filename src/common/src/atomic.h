/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    atomic.h
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-02-11
 * @brief   Cross-platform atomic operations
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

#ifndef COMMON_ATOMIC_H_
#define COMMON_ATOMIC_H_

#include "platform.h"

#ifdef _WIN32
#include "atomic_win32.h"
#else
#include "atomic_gcc.h"
#endif

#endif // COMMON_ATOMIC_H_