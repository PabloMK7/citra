/*
 *    arm
 *    armcpu.h
 *
 *    Copyright (C) 2003, 2004 Sebastian Biallas (sb@biallas.net)
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License version 2 as
 *    published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __ARM_CPU_H__
#define __ARM_CPU_H__
//#include <skyeye_thread.h>
//#include <skyeye_obj.h>
//#include <skyeye_mach.h>
//#include <skyeye_exec.h>

#include <stddef.h>
#include <stdio.h>

#include "common/thread.h"


typedef struct ARM_CPU_State_s {
    ARMul_State * core;
    uint32_t core_num;
    /* The core id that boot from
     */
    uint32_t boot_core_id;
}ARM_CPU_State;

//static ARM_CPU_State* get_current_cpu(){
//    machine_config_t* mach = get_current_mach();
//    /* Casting a conf_obj_t to ARM_CPU_State type */
//    ARM_CPU_State* cpu = (ARM_CPU_State*)mach->cpu_data->obj;
//
//    return cpu;
//}

/**
* @brief Get the core instance boot from
*
* @return
*/
//static ARMul_State* get_boot_core(){
//    ARM_CPU_State* cpu = get_current_cpu();
//    return &cpu->core[cpu->boot_core_id];
//}
/**
* @brief Get the instance of running core
*
* @return the core instance
*/
//static ARMul_State* get_current_core(){
//    /* Casting a conf_obj_t to ARM_CPU_State type */
//    int id = Common::CurrentThreadId();
//    /* If thread is not in running mode, we should give the boot core */
//    if(get_thread_state(id) != Running_state){
//        return get_boot_core();
//    }
//    /* Judge if we are running in paralell or sequenial */
//    if(thread_exist(id)){
//        conf_object_t* conf_obj = get_current_exec_priv(id);
//        return (ARMul_State*)get_cast_conf_obj(conf_obj, "arm_core_t");
//    }
//
//    return NULL;
//}

#define CURRENT_CORE get_current_core()

#endif

