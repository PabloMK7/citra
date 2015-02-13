// Copyright 2012 Michael Kang, 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/arm/skyeye_common/armdefs.h"

void switch_mode(ARMul_State* core, uint32_t mode) {
    if (core->Mode == mode)
        return;

    if (mode != USERBANK) {
        switch (core->Mode) {
        case SYSTEM32MODE: // Shares registers with user mode
        case USER32MODE:
            core->Reg_usr[0] = core->Reg[13];
            core->Reg_usr[1] = core->Reg[14];
            break;
        case IRQ32MODE:
            core->Reg_irq[0] = core->Reg[13];
            core->Reg_irq[1] = core->Reg[14];
            core->Spsr[IRQBANK] = core->Spsr_copy;
            break;
        case SVC32MODE:
            core->Reg_svc[0] = core->Reg[13];
            core->Reg_svc[1] = core->Reg[14];
            core->Spsr[SVCBANK] = core->Spsr_copy;
            break;
        case ABORT32MODE:
            core->Reg_abort[0] = core->Reg[13];
            core->Reg_abort[1] = core->Reg[14];
            core->Spsr[ABORTBANK] = core->Spsr_copy;
            break;
        case UNDEF32MODE:
            core->Reg_undef[0] = core->Reg[13];
            core->Reg_undef[1] = core->Reg[14];
            core->Spsr[UNDEFBANK] = core->Spsr_copy;
            break;
        case FIQ32MODE:
            core->Reg_firq[0] = core->Reg[13];
            core->Reg_firq[1] = core->Reg[14];
            core->Spsr[FIQBANK] = core->Spsr_copy;
            break;
        }

        switch (mode) {
        case USER32MODE:
            core->Reg[13] = core->Reg_usr[0];
            core->Reg[14] = core->Reg_usr[1];
            core->Bank = USERBANK;
            break;
        case IRQ32MODE:
            core->Reg[13] = core->Reg_irq[0];
            core->Reg[14] = core->Reg_irq[1];
            core->Spsr_copy = core->Spsr[IRQBANK];
            core->Bank = IRQBANK;
            break;
        case SVC32MODE:
            core->Reg[13] = core->Reg_svc[0];
            core->Reg[14] = core->Reg_svc[1];
            core->Spsr_copy = core->Spsr[SVCBANK];
            core->Bank = SVCBANK;
            break;
        case ABORT32MODE:
            core->Reg[13] = core->Reg_abort[0];
            core->Reg[14] = core->Reg_abort[1];
            core->Spsr_copy = core->Spsr[ABORTBANK];
            core->Bank = ABORTBANK;
            break;
        case UNDEF32MODE:
            core->Reg[13] = core->Reg_undef[0];
            core->Reg[14] = core->Reg_undef[1];
            core->Spsr_copy = core->Spsr[UNDEFBANK];
            core->Bank = UNDEFBANK;
            break;
        case FIQ32MODE:
            core->Reg[13] = core->Reg_firq[0];
            core->Reg[14] = core->Reg_firq[1];
            core->Spsr_copy = core->Spsr[FIQBANK];
            core->Bank = FIQBANK;
            break;
        case SYSTEM32MODE: // Shares registers with user mode.
            core->Reg[13] = core->Reg_usr[0];
            core->Reg[14] = core->Reg_usr[1];
            core->Bank = SYSTEMBANK;
            break;
        }

        // Set the mode bits in the APSR
        core->Cpsr = (core->Cpsr & ~core->Mode) | mode;
        core->Mode = mode;
    }
}
