// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include <string.h>

#include "common/common.h"

#include "core/core.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

ObjectPool g_object_pool;

ObjectPool::ObjectPool() {
    memset(occupied, 0, sizeof(bool) * MAX_COUNT);
    next_id = INITIAL_NEXT_ID;
}

Handle ObjectPool::Create(Object* obj, int range_bottom, int range_top) {
    if (range_top > MAX_COUNT) {
        range_top = MAX_COUNT;
    }
    if (next_id >= range_bottom && next_id < range_top) {
        range_bottom = next_id++;
    }
    for (int i = range_bottom; i < range_top; i++) {
        if (!occupied[i]) {
            occupied[i] = true;
            pool[i] = obj;
            pool[i]->handle = i + HANDLE_OFFSET;
            return i + HANDLE_OFFSET;
        }
    }
    ERROR_LOG(HLE, "Unable to allocate kernel object, too many objects slots in use.");
    return 0;
}

bool ObjectPool::IsValid(Handle handle) {
    int index = handle - HANDLE_OFFSET;
    if (index < 0)
        return false;
    if (index >= MAX_COUNT)
        return false;

    return occupied[index];
}

void ObjectPool::Clear() {
    for (int i = 0; i < MAX_COUNT; i++) {
        //brutally clear everything, no validation
        if (occupied[i])
            delete pool[i];
        occupied[i] = false;
    }
    memset(pool, 0, sizeof(Object*)*MAX_COUNT);
    next_id = INITIAL_NEXT_ID;
}

Object* &ObjectPool::operator [](Handle handle)
{
    _dbg_assert_msg_(KERNEL, IsValid(handle), "GRABBING UNALLOCED KERNEL OBJ");
    return pool[handle - HANDLE_OFFSET];
}

void ObjectPool::List() {
    for (int i = 0; i < MAX_COUNT; i++) {
        if (occupied[i]) {
            if (pool[i]) {
                INFO_LOG(KERNEL, "KO %i: %s \"%s\"", i + HANDLE_OFFSET, pool[i]->GetTypeName(), 
                    pool[i]->GetName());
            }
        }
    }
}

int ObjectPool::GetCount() {
    int count = 0;
    for (int i = 0; i < MAX_COUNT; i++) {
        if (occupied[i])
            count++;
    }
    return count;
}

Object* ObjectPool::CreateByIDType(int type) {
    // Used for save states.  This is ugly, but what other way is there?
    switch (type) {
    //case SCE_KERNEL_TMID_Alarm:
    //    return __KernelAlarmObject();
    //case SCE_KERNEL_TMID_EventFlag:
    //    return __KernelEventFlagObject();
    //case SCE_KERNEL_TMID_Mbox:
    //    return __KernelMbxObject();
    //case SCE_KERNEL_TMID_Fpl:
    //    return __KernelMemoryFPLObject();
    //case SCE_KERNEL_TMID_Vpl:
    //    return __KernelMemoryVPLObject();
    //case PPSSPP_KERNEL_TMID_PMB:
    //    return __KernelMemoryPMBObject();
    //case PPSSPP_KERNEL_TMID_Module:
    //    return __KernelModuleObject();
    //case SCE_KERNEL_TMID_Mpipe:
    //    return __KernelMsgPipeObject();
    //case SCE_KERNEL_TMID_Mutex:
    //    return __KernelMutexObject();
    //case SCE_KERNEL_TMID_LwMutex:
    //    return __KernelLwMutexObject();
    //case SCE_KERNEL_TMID_Semaphore:
    //    return __KernelSemaphoreObject();
    //case SCE_KERNEL_TMID_Callback:
    //    return __KernelCallbackObject();
    //case SCE_KERNEL_TMID_Thread:
    //    return __KernelThreadObject();
    //case SCE_KERNEL_TMID_VTimer:
    //    return __KernelVTimerObject();
    //case SCE_KERNEL_TMID_Tlspl:
    //    return __KernelTlsplObject();
    //case PPSSPP_KERNEL_TMID_File:
    //    return __KernelFileNodeObject();
    //case PPSSPP_KERNEL_TMID_DirList:
    //    return __KernelDirListingObject();

    default:
        ERROR_LOG(COMMON, "Unable to load state: could not find object type %d.", type);
        return NULL;
    }
}

void Init() {
    Kernel::ThreadingInit();
}

void Shutdown() {
    Kernel::ThreadingShutdown();
}

/**
 * Loads executable stored at specified address
 * @entry_point Entry point in memory of loaded executable
 * @return True on success, otherwise false
 */
bool LoadExec(u32 entry_point) {
    Init();
    
    Core::g_app_core->SetPC(entry_point);

    // 0x30 is the typical main thread priority I've seen used so far
    Handle thread = Kernel::SetupMainThread(0x30);

    return true;
}

} // namespace
