// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include <memory>

#include "common/common_types.h"

#include "core/arm/arm_interface.h"
#include "core/arm/skyeye_common/armdefs.h"

class ARM_DynCom final : virtual public ARM_Interface {
public:

    ARM_DynCom();
    ~ARM_DynCom();

    /**
     * Set the Program Counter to an address
     * @param pc Address to set PC to
     */
    void SetPC(u32 pc) override;

    /*
     * Get the current Program Counter
     * @return Returns current PC
     */
    u32 GetPC() const;

    /**
     * Get an ARM register
     * @param index Register index (0-15)
     * @return Returns the value in the register
     */
    u32 GetReg(int index) const;

    /**
     * Set an ARM register
     * @param index Register index (0-15)
     * @param value Value to set register to
     */
    void SetReg(int index, u32 value) override;

    /**
     * Get the current CPSR register
     * @return Returns the value of the CPSR register
     */
    u32 GetCPSR() const;

    /**
     * Set the current CPSR register
     * @param cpsr Value to set CPSR to
     */
    void SetCPSR(u32 cpsr) override;

    /**
     * Returns the number of clock ticks since the last reset
     * @return Returns number of clock ticks
     */
    u64 GetTicks() const;

    /**
     * Saves the current CPU context
     * @param ctx Thread context to save
     */
    void SaveContext(ThreadContext& ctx) override;

    /**
     * Loads a CPU context
     * @param ctx Thread context to load
     */
    void LoadContext(const ThreadContext& ctx) override;

    /// Prepare core for thread reschedule (if needed to correctly handle state)
    void PrepareReschedule() override;

    /**
     * Executes the given number of instructions
     * @param num_instructions Number of instructions to executes
     */
    void ExecuteInstructions(int num_instructions) override;

private:

    std::unique_ptr<ARMul_State> state;
    u64 ticks;

};
