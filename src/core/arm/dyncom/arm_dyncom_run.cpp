/* Copyright (C)
* 2011 - Michael.Kang blackfin.kang@gmail.com
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/
/**
* @file arm_dyncom_run.cpp
* @brief The dyncom run implementation for arm
* @author Michael.Kang blackfin.kang@gmail.com
* @version 78.77
* @date 2011-11-20
*/

#include <assert.h>

#include "core/arm/skyeye_common/armdefs.h"

void switch_mode(arm_core_t *core, uint32_t mode)
{
    uint32_t tmp1, tmp2;
    if (core->Mode == mode) {
        //Mode not changed.
        //printf("mode not changed\n");
        return;
    }
    //printf("%d --->>> %d\n", core->Mode, mode);
    //printf("In %s, Cpsr=0x%x, R15=0x%x, last_pc=0x%x, cpsr=0x%x, spsr_copy=0x%x, icounter=%lld\n", __FUNCTION__, core->Cpsr, core->Reg[15], core->last_pc, core->Cpsr, core->Spsr_copy, core->icounter);
    if (mode != USERBANK) {
        switch (core->Mode) {
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

        }
        core->Mode = mode;
        //printf("In %si end, Cpsr=0x%x, R15=0x%x, last_pc=0x%x, cpsr=0x%x, spsr_copy=0x%x, icounter=%lld\n", __FUNCTION__, core->Cpsr, core->Reg[15], core->last_pc, core->Cpsr, core->Spsr_copy, core->icounter);
        //printf("\n--------------------------------------\n");
    }
    else {
        printf("user mode\n");
        exit(-2);
    }
}
