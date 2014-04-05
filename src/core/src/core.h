/**
 * Copyright (C) 2013 Citrus Emulator
 *
 * @file    core.h
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2013-09-04
 * @brief   Core of emulator
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

#ifndef CORE_CORE_H_
#define CORE_CORE_H_

#include "arm/arm_interface.h"
#include "arm/interpreter/armdefs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Core {

extern ARM_Interface*   g_app_core;     ///< ARM11 application core
extern ARM_Interface*   g_sys_core;     ///< ARM11 system (OS) core

////////////////////////////////////////////////////////////////////////////////////////////////////

/// Start the core
void Start();

/// Run the core CPU loop
void RunLoop();

/// Step the CPU one instruction
void SingleStep();

/// Halt the core
void Halt(const char *msg);

/// Kill the core
void Stop();

/// Initialize the core
int Init();

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // CORE_CORE_H_
