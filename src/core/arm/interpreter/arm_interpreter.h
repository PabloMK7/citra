/**
* Copyright (C) 2013 Citrus Emulator
*
* @file    arm_interpreter.h
* @author  bunnei
* @date    2014-04-04
* @brief   ARM interface instance for SkyEye interprerer
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

#include "common/common.h"

#include "core/arm/arm_interface.h"
#include "core/arm/interpreter/armdefs.h"
#include "core/arm/interpreter/armemu.h"

class ARM_Interpreter : virtual public ARM_Interface {
public:
    ARM_Interpreter();
    ~ARM_Interpreter();

    void ExecuteInstruction();

    void SetPC(u32 pc);

    u32 PC();

    u32 Reg(int index);

    u32 CPSR();

    u64 GetTicks() {
        return ARMul_Time(state);
    }

private:
    ARMul_State* state;

    DISALLOW_COPY_AND_ASSIGN(ARM_Interpreter);
};
