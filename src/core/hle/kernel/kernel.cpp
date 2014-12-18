// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>

#include "common/common.h"

#include "core/core.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

Handle g_main_thread = 0;
ObjectPool g_object_pool;
u64 g_program_id = 0;

ObjectPool::ObjectPool() {
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
    LOG_ERROR(Kernel, "Unable to allocate kernel object, too many objects slots in use.");
    return 0;
}

bool ObjectPool::IsValid(Handle handle) const {
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
    pool.fill(nullptr);
    next_id = INITIAL_NEXT_ID;
}

Object* &ObjectPool::operator [](Handle handle)
{
    _dbg_assert_msg_(Kernel, IsValid(handle), "GRABBING UNALLOCED KERNEL OBJ");
    return pool[handle - HANDLE_OFFSET];
}

void ObjectPool::List() {
    for (int i = 0; i < MAX_COUNT; i++) {
        if (occupied[i]) {
            if (pool[i]) {
                LOG_DEBUG(Kernel, "KO %i: %s \"%s\"", i + HANDLE_OFFSET, pool[i]->GetTypeName().c_str(),
                    pool[i]->GetName().c_str());
            }
        }
    }
}

int ObjectPool::GetCount() const {
    return std::count(occupied.begin(), occupied.end(), true);
}

Object* ObjectPool::CreateByIDType(int type) {
    LOG_ERROR(Kernel, "Unimplemented: %d.", type);
    return nullptr;
}

/// Initialize the kernel
void Init() {
    Kernel::ThreadingInit();
}

/// Shutdown the kernel
void Shutdown() {
    Kernel::ThreadingShutdown();

    g_object_pool.Clear(); // Free all kernel objects
}

/**
 * Loads executable stored at specified address
 * @entry_point Entry point in memory of loaded executable
 * @return True on success, otherwise false
 */
bool LoadExec(u32 entry_point) {
    Core::g_app_core->SetPC(entry_point);

    // 0x30 is the typical main thread priority I've seen used so far
    g_main_thread = Kernel::SetupMainThread(0x30);

    return true;
}

} // namespace
