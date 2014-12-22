// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>

#include "common/common.h"

#include "core/arm/arm_interface.h"
#include "core/core.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/timer.h"

namespace Kernel {

Handle g_main_thread = 0;
HandleTable g_handle_table;
u64 g_program_id = 0;

HandleTable::HandleTable() {
    next_generation = 1;
    Clear();
}

ResultVal<Handle> HandleTable::Create(Object* obj) {
    _dbg_assert_(Kernel, obj != nullptr);

    u16 slot = next_free_slot;
    if (slot >= generations.size()) {
        LOG_ERROR(Kernel, "Unable to allocate Handle, too many slots in use.");
        return ERR_OUT_OF_HANDLES;
    }
    next_free_slot = generations[slot];

    u16 generation = next_generation++;

    // Overflow count so it fits in the 15 bits dedicated to the generation in the handle.
    // CTR-OS doesn't use generation 0, so skip straight to 1.
    if (next_generation >= (1 << 15)) next_generation = 1;

    generations[slot] = generation;
    intrusive_ptr_add_ref(obj);
    objects[slot] = obj;

    Handle handle = generation | (slot << 15);
    obj->handle = handle;
    return MakeResult<Handle>(handle);
}

ResultVal<Handle> HandleTable::Duplicate(Handle handle) {
    Object* object = GetGeneric(handle);
    if (object == nullptr) {
        LOG_ERROR(Kernel, "Tried to duplicate invalid handle: %08X", handle);
        return ERR_INVALID_HANDLE;
    }
    return Create(object);
}

ResultCode HandleTable::Close(Handle handle) {
    if (!IsValid(handle))
        return ERR_INVALID_HANDLE;

    size_t slot = GetSlot(handle);
    u16 generation = GetGeneration(handle);

    intrusive_ptr_release(objects[slot]);
    objects[slot] = nullptr;

    generations[generation] = next_free_slot;
    next_free_slot = slot;
    return RESULT_SUCCESS;
}

bool HandleTable::IsValid(Handle handle) const {
    size_t slot = GetSlot(handle);
    u16 generation = GetGeneration(handle);

    return slot < MAX_COUNT && objects[slot] != nullptr && generations[slot] == generation;
}

Object* HandleTable::GetGeneric(Handle handle) const {
    if (handle == CurrentThread) {
        // TODO(yuriks) Directly return the pointer once this is possible.
        handle = GetCurrentThreadHandle();
    } else if (handle == CurrentProcess) {
        LOG_ERROR(Kernel, "Current process (%08X) pseudo-handle not supported", CurrentProcess);
        return nullptr;
    }

    if (!IsValid(handle)) {
        return nullptr;
    }
    return objects[GetSlot(handle)];
}

void HandleTable::Clear() {
    for (size_t i = 0; i < MAX_COUNT; ++i) {
        generations[i] = i + 1;
        if (objects[i] != nullptr)
            intrusive_ptr_release(objects[i]);
        objects[i] = nullptr;
    }
    next_free_slot = 0;
}

/// Initialize the kernel
void Init() {
    Kernel::ThreadingInit();
    Kernel::TimersInit();
}

/// Shutdown the kernel
void Shutdown() {
    Kernel::ThreadingShutdown();
    Kernel::TimersShutdown();
    g_handle_table.Clear(); // Free all kernel objects
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
    // Setup the idle thread
    Kernel::SetupIdleThread();

    return true;
}

} // namespace
