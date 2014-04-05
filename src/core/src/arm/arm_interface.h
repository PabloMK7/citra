/**
 * Copyright (C) 2013 Citrus Emulator
 *
 * @file    arm_interface.h
 * @author  bunnei 
 * @date    2014-04-04
 * @brief   Generic ARM CPU core interface
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

#include "common_types.h"

/// Generic ARM11 CPU interface
class ARM_Interface {
public:
    ARM_Interface() {
    }

    ~ARM_Interface() {
    }

    virtual void ExecuteInstruction() = 0;
    
    virtual void SetPC(u32 pc) = 0;

    virtual u32 PC() = 0;

    virtual u32 Reg(int index) = 0;

    virtual u32 CPSR() = 0;
};
