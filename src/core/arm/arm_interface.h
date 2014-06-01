// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include "common/common.h"
#include "common/common_types.h"

#include "core/hle/svc.h"

/// Generic ARM11 CPU interface
class ARM_Interface : NonCopyable {
public:
    ARM_Interface() {
        num_instructions = 0;
    }

    ~ARM_Interface() {
    }

    /**
     * Runs the CPU for the given number of instructions
     * @param num_instructions Number of instructions to run
     */
    void Run(int num_instructions) {
        ExecuteInstructions(num_instructions);
        this->num_instructions += num_instructions;
    }

    /// Step CPU by one instruction
    void Step() {
        Run(1);
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
     * Set the current CPSR register
     * @param cpsr Value to set CPSR to
     */
    virtual void SetCPSR(u32 cpsr) = 0;

    /**
     * Returns the number of clock ticks since the last rese
     * @return Returns number of clock ticks
     */
    virtual u64 GetTicks() const = 0;

    /**
     * Saves the current CPU context
     * @param ctx Thread context to save
     */
    virtual void SaveContext(ThreadContext& ctx) = 0;

    /**
     * Loads a CPU context
     * @param ctx Thread context to load
     */
    virtual void LoadContext(const ThreadContext& ctx) = 0;

    /// Getter for num_instructions
    u64 GetNumInstructions() {
        return num_instructions;
    }

protected:
    
    /**
     * Executes the given number of instructions
     * @param num_instructions Number of instructions to executes
     */
    virtual void ExecuteInstructions(int num_instructions) = 0;

private:

    u64 num_instructions; ///< Number of instructions executed

};
