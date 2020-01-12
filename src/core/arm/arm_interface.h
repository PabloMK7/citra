// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <boost/serialization/split_member.hpp>
#include "common/common_types.h"
#include "core/arm/skyeye_common/arm_regformat.h"
#include "core/arm/skyeye_common/vfp/asm_vfp.h"

/// Generic ARM11 CPU interface
class ARM_Interface : NonCopyable {
public:
    virtual ~ARM_Interface() {}

    class ThreadContext {
        friend class boost::serialization::access;

        template <class Archive>
        void save(Archive& ar, const unsigned int file_version) const {
            for (auto i = 0; i < 16; i++) {
                auto r = GetCpuRegister(i);
                ar << r;
            }
            for (auto i = 0; i < 16; i++) {
                auto r = GetFpuRegister(i);
                ar << r;
            }
            auto r1 = GetCpsr();
            ar << r1;
            auto r2 = GetFpscr();
            ar << r2;
            auto r3 = GetFpexc();
            ar << r3;
        }

        template <class Archive>
        void load(Archive& ar, const unsigned int file_version) {
            u32 r;
            for (auto i = 0; i < 16; i++) {
                ar >> r;
                SetCpuRegister(i, r);
            }
            for (auto i = 0; i < 16; i++) {
                ar >> r;
                SetFpuRegister(i, r);
            }
            ar >> r;
            SetCpsr(r);
            ar >> r;
            SetFpscr(r);
            ar >> r;
            SetFpexc(r);
        }

        BOOST_SERIALIZATION_SPLIT_MEMBER()
    public:
        virtual ~ThreadContext() = default;

        virtual void Reset() = 0;
        virtual u32 GetCpuRegister(std::size_t index) const = 0;
        virtual void SetCpuRegister(std::size_t index, u32 value) = 0;
        virtual u32 GetCpsr() const = 0;
        virtual void SetCpsr(u32 value) = 0;
        virtual u32 GetFpuRegister(std::size_t index) const = 0;
        virtual void SetFpuRegister(std::size_t index, u32 value) = 0;
        virtual u32 GetFpscr() const = 0;
        virtual void SetFpscr(u32 value) = 0;
        virtual u32 GetFpexc() const = 0;
        virtual void SetFpexc(u32 value) = 0;

        u32 GetStackPointer() const {
            return GetCpuRegister(13);
        }
        void SetStackPointer(u32 value) {
            return SetCpuRegister(13, value);
        }

        u32 GetLinkRegister() const {
            return GetCpuRegister(14);
        }
        void SetLinkRegister(u32 value) {
            return SetCpuRegister(14, value);
        }

        u32 GetProgramCounter() const {
            return GetCpuRegister(15);
        }
        void SetProgramCounter(u32 value) {
            return SetCpuRegister(15, value);
        }
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

    /// Notify CPU emulation that page tables have changed
    virtual void PageTableChanged() = 0;

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
     * Creates a CPU context
     * @note The created context may only be used with this instance.
     */
    virtual std::unique_ptr<ThreadContext> NewContext() const = 0;

    /**
     * Saves the current CPU context
     * @param ctx Thread context to save
     */
    virtual void SaveContext(const std::unique_ptr<ThreadContext>& ctx) = 0;

    /**
     * Loads a CPU context
     * @param ctx Thread context to load
     */
    virtual void LoadContext(const std::unique_ptr<ThreadContext>& ctx) = 0;

    /// Prepare core for thread reschedule (if needed to correctly handle state)
    virtual void PrepareReschedule() = 0;

private:
    friend class boost::serialization::access;

    template <class Archive>
    void save(Archive& ar, const unsigned int file_version) const {
        for (auto i = 0; i < 15; i++) {
            auto r = GetReg(i);
            ar << r;
        }
        auto pc = GetPC();
        ar << pc;
        auto cpsr = GetCPSR();
        ar << cpsr;
        for (auto i = 0; i < 32; i++) {
            auto r = GetVFPReg(i);
            ar << r;
        }
        for (auto i = 0; i < VFPSystemRegister::VFP_SYSTEM_REGISTER_COUNT; i++) {
            auto r = GetVFPSystemReg(static_cast<VFPSystemRegister>(i));
            ar << r;
        }
        for (auto i = 0; i < CP15Register::CP15_REGISTER_COUNT; i++) {
            auto r = GetCP15Register(static_cast<CP15Register>(i));
            ar << r;
        }
    }

    template <class Archive>
    void load(Archive& ar, const unsigned int file_version) {
        u32 r;
        for (auto i = 0; i < 15; i++) {
            ar >> r;
            SetReg(i, r);
        }
        ar >> r;
        SetPC(r);
        ar >> r;
        SetCPSR(r);
        for (auto i = 0; i < 32; i++) {
            ar >> r;
            SetVFPReg(i, r);
        }
        for (auto i = 0; i < VFPSystemRegister::VFP_SYSTEM_REGISTER_COUNT; i++) {
            ar >> r;
            SetVFPSystemReg(static_cast<VFPSystemRegister>(i), r);
        }
        for (auto i = 0; i < CP15Register::CP15_REGISTER_COUNT; i++) {
            ar >> r;
            SetCP15Register(static_cast<CP15Register>(i), r);
        }
        ClearInstructionCache();
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER()
};
