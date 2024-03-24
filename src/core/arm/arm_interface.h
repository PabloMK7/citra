// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include "common/common_types.h"
#include "core/arm/skyeye_common/arm_regformat.h"
#include "core/arm/skyeye_common/vfp/asm_vfp.h"
#include "core/core_timing.h"
#include "core/memory.h"

namespace Memory {
struct PageTable;
};

namespace Core {

/// Generic ARM11 CPU interface
class ARM_Interface : NonCopyable {
public:
    explicit ARM_Interface(u32 id, std::shared_ptr<Core::Timing::Timer> timer)
        : timer(timer), id(id){};
    virtual ~ARM_Interface() {}

    struct ThreadContext {
        u32 GetStackPointer() const {
            return cpu_registers[13];
        }
        void SetStackPointer(u32 value) {
            cpu_registers[13] = value;
        }

        u32 GetLinkRegister() const {
            return cpu_registers[14];
        }
        void SetLinkRegister(u32 value) {
            cpu_registers[14] = value;
        }

        u32 GetProgramCounter() const {
            return cpu_registers[15];
        }
        void SetProgramCounter(u32 value) {
            cpu_registers[15] = value;
        }

        std::array<u32, 16> cpu_registers{};
        u32 cpsr{};
        std::array<u32, 64> fpu_registers{};
        u32 fpscr{};
        u32 fpexc{};
    };

    /// Runs the CPU until an event happens
    virtual void Run() = 0;

    /// Step CPU by one instruction
    virtual void Step() = 0;

    /// Clear all instruction cache
    virtual void ClearInstructionCache() = 0;

    /**
     * Invalidate the code cache at a range of addresses.
     * @param start_address The starting address of the range to invalidate.
     * @param length The length (in bytes) of the range to invalidate.
     */
    virtual void InvalidateCacheRange(u32 start_address, std::size_t length) = 0;

    /// Clears the exclusive monitor's state.
    virtual void ClearExclusiveState() = 0;

    /// Notify CPU emulation that page tables have changed
    virtual void SetPageTable(const std::shared_ptr<Memory::PageTable>& page_table) = 0;

    /**
     * Set the Program Counter to an address
     * @param addr Address to set PC to
     */
    virtual void SetPC(u32 addr) = 0;

    /**
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
     * Gets the value of a VFP register
     * @param index Register index (0-31)
     * @return Returns the value in the register
     */
    virtual u32 GetVFPReg(int index) const = 0;

    /**
     * Sets a VFP register to the given value
     * @param index Register index (0-31)
     * @param value Value to set register to
     */
    virtual void SetVFPReg(int index, u32 value) = 0;

    /**
     * Gets the current value within a given VFP system register
     * @param reg The VFP system register
     * @return The value within the VFP system register
     */
    virtual u32 GetVFPSystemReg(VFPSystemRegister reg) const = 0;

    /**
     * Sets the VFP system register to the given value
     * @param reg   The VFP system register
     * @param value Value to set the VFP system register to
     */
    virtual void SetVFPSystemReg(VFPSystemRegister reg, u32 value) = 0;

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
     * Gets the value stored in a CP15 register.
     * @param reg The CP15 register to retrieve the value from.
     * @return the value stored in the given CP15 register.
     */
    virtual u32 GetCP15Register(CP15Register reg) const = 0;

    /**
     * Stores the given value into the indicated CP15 register.
     * @param reg   The CP15 register to store the value into.
     * @param value The value to store into the CP15 register.
     */
    virtual void SetCP15Register(CP15Register reg, u32 value) = 0;

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

    /// Prepare core for thread reschedule (if needed to correctly handle state)
    virtual void PrepareReschedule() = 0;

    Core::Timing::Timer& GetTimer() {
        return *timer;
    }

    const Core::Timing::Timer& GetTimer() const {
        return *timer;
    }

    u32 GetID() const {
        return id;
    }

protected:
    std::shared_ptr<Core::Timing::Timer> timer;
    u32 id;
};

} // namespace Core
