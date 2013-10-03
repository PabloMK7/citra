/**
 * Copyright (C) 2013 Citrus Emulator
 *
 * @file    core.cpp
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

#include "log.h"
#include "core.h"

namespace Core {

/// Start the core
void Start() {
	// TODO(ShizZy): ImplementMe
}

/// Run the core CPU loop
void RunLoop() {
	// TODO(ShizZy): ImplementMe
}

/// Step the CPU one instruction
void SingleStep() {
}

/// Halt the core
void Halt(const char *msg) {
	// TODO(ShizZy): ImplementMe
}

/// Kill the core
void Stop() {
	// TODO(ShizZy): ImplementMe
}

/// Initialize the core
int Init() {
	NOTICE_LOG(MASTER_LOG, "Core initialized OK");
	return 0;
}

} // namespace
