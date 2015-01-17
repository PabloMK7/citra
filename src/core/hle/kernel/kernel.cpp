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

SharedPtr<Thread> g_main_thread = nullptr;
HandleTable g_handle_table;
u64 g_program_id = 0;

void WaitObject::AddWaitingThread(Thread* thread) {
    auto itr = std::find(waiting_threads.begin(), waiting_threads.end(), thread);
    if (itr == waiting_threads.end())
        waiting_threads.push_back(thread);
}

void WaitObject::RemoveWaitingThread(Thread* thread) {
    auto itr = std::find(waiting_threads.begin(), waiting_threads.end(), thread);
    if (itr != waiting_threads.end())
        waiting_threads.erase(itr);
}

Thread* WaitObject::ReleaseNextThread() {
    if (waiting_threads.empty())
        return nullptr;

    auto next_thread = waiting_threads.front();

    next_thread->ReleaseFromWait(this);
    waiting_threads.erase(waiting_threads.begin());

    return next_thread.get();
}

void WaitObject::ReleaseAllWaitingThreads() {
    auto waiting_threads_copy = waiting_threads;

    for (auto thread : waiting_threads_copy)
        thread->ReleaseWaitObject(this);

    waiting_threads.clear();
}

HandleTable::HandleTable() {
    next_generation = 1;
    Clear();
}

ResultVal<Handle> HandleTable::Create(SharedPtr<Object> obj) {
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

    Handle handle = generation | (slot << 15);
    if (obj->handle == INVALID_HANDLE)
        obj->handle = handle;

    generations[slot] = generation;
    objects[slot] = std::move(obj);

    return MakeResult<Handle>(handle);
}

ResultVal<Handle> HandleTable::Duplicate(Handle handle) {
    SharedPtr<Object> object = GetGeneric(handle);
    if (object == nullptr) {
        LOG_ERROR(Kernel, "Tried to duplicate invalid handle: %08X", handle);
        return ERR_INVALID_HANDLE;
    }
    return Create(std::move(object));
}

ResultCode HandleTable::Close(Handle handle) {
    if (!IsValid(handle))
        return ERR_INVALID_HANDLE;

    size_t slot = GetSlot(handle);
    u16 generation = GetGeneration(handle);

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

SharedPtr<Object> HandleTable::GetGeneric(Handle handle) const {
    if (handle == CurrentThread) {
        return GetCurrentThread();
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
    g_main_thread = Kernel::SetupMainThread(0x30, Kernel::DEFAULT_STACK_SIZE);
    // Setup the idle thread
    Kernel::SetupIdleThread();

    return true;
}

} // namespace
