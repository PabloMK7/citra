// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

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

    u32 GetPC() const;

    u32 GetReg(int index) const;

    u32 GetCPSR() const;

    u64 GetTicks() const;

private:
    ARMul_State* m_state;

    DISALLOW_COPY_AND_ASSIGN(ARM_Interpreter);
};
