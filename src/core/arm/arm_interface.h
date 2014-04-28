// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include "common/common.h"
#include "common/common_types.h"

/// Generic ARM11 CPU interface
class ARM_Interface : NonCopyable {
public:
    ARM_Interface() {
        m_num_instructions = 0;
    }

    ~ARM_Interface() {
    }

    /// Step CPU by one instruction
    void Step() {
        ExecuteInstruction();
        m_num_instructions++;
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
    virtual u32 GetPC() const = 0;

    /**
     * Get an ARM register
     * @param index Register index (0-15)
     * @return Returns the value in the register
     */
    virtual u32 GetReg(int index) const = 0;

    /**
     * Set an ARM register
     * @param index Register index (0-15)
     * @param value Value to set register to
     */
    virtual void SetReg(int index, u32 value) = 0;

    /**
     * Get the current CPSR register
     * @return Returns the value of the CPSR register
     */
    virtual u32 GetCPSR() const = 0;  

    /**
     * Returns the number of clock ticks since the last rese
     * @return Returns number of clock ticks
     */
    virtual u64 GetTicks() const = 0;

    /// Getter for m_num_instructions
    u64 GetNumInstructions() {
        return m_num_instructions;
    }

protected:
    
    /// Execture next instruction
    virtual void ExecuteInstruction() = 0;

private:

    u64 m_num_instructions;                     ///< Number of instructions executed

};
