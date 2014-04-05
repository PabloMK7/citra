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

#include "common.h"
#include "common_types.h"

/// Generic ARM11 CPU interface
class ARM_Interface {
public:
    ARM_Interface() {
        num_instructions_ = 0;
    }

    ~ARM_Interface() {
    }

    /// Step CPU by one instruction
    void Step() {
        ExecuteInstruction();
        num_instructions_++;
    }
 
    /**
     * Set the Program Counter to an address
     * @param addr Address to set PC to
     */
    virtual void SetPC(u32 addr) = 0;

    /*
     * Get the current Program Counter
     * @return Returns current PC
     */
    virtual u32 PC() = 0;

    /**
     * Get an ARM register
     * @param index Register index (0-15)
     * @return Returns the value in the register
     */
    virtual u32 Reg(int index) = 0;

    /**
     * Get the current CPSR register
     * @return Returns the value of the CPSR register
     */
    virtual u32 CPSR() = 0;  

    /**
     * Returns the number of clock ticks since the last rese
     * @return Returns number of clock ticks
     */
    virtual u64 GetTicks() = 0;

    /// Getter for num_instructions_
    u64 num_instructions() { return num_instructions_; }

private:
    
    /// Execture next instruction
    virtual void ExecuteInstruction() = 0;

    u64 num_instructions_;  ///< Number of instructions executed

    DISALLOW_COPY_AND_ASSIGN(ARM_Interface);
};
