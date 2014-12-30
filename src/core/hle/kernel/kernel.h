// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include "common/common.h"
#include "core/hle/result.h"

typedef u32 Handle;
typedef s32 Result;

const Handle INVALID_HANDLE = 0;

namespace Kernel {

// TODO: Verify code
const ResultCode ERR_OUT_OF_HANDLES(ErrorDescription::OutOfMemory, ErrorModule::Kernel,
        ErrorSummary::OutOfResource, ErrorLevel::Temporary);
// TOOD: Verify code
const ResultCode ERR_INVALID_HANDLE = InvalidHandle(ErrorModule::Kernel);

enum KernelHandle : Handle {
    CurrentThread   = 0xFFFF8000,
    CurrentProcess  = 0xFFFF8001,
};

enum class HandleType : u32 {
    Unknown         = 0,
    Port            = 1,
    Session         = 2,
    Event           = 3,
    Mutex           = 4,
    SharedMemory    = 5,
    Redirection     = 6,
    Thread          = 7,
    Process         = 8,
    AddressArbiter  = 9,
    Semaphore       = 10,
};

enum {
    DEFAULT_STACK_SIZE  = 0x4000,
};

class HandleTable;

class Object : NonCopyable {
    friend class HandleTable;
    u32 handle;
public:
    virtual ~Object() {}
    Handle GetHandle() const { return handle; }
    virtual std::string GetTypeName() const { return "[BAD KERNEL OBJECT TYPE]"; }
    virtual std::string GetName() const { return "[UNKNOWN KERNEL OBJECT]"; }
    virtual Kernel::HandleType GetHandleType() const = 0;

    /**
     * Wait for kernel object to synchronize.
     * @return True if the current thread should wait as a result of the wait
     */
    virtual ResultVal<bool> WaitSynchronization() {
        LOG_ERROR(Kernel, "(UNIMPLEMENTED)");
        return UnimplementedFunction(ErrorModule::Kernel);
    }

private:
    friend void intrusive_ptr_add_ref(Object*);
    friend void intrusive_ptr_release(Object*);

    unsigned int ref_count = 0;
};

// Special functions that will later be used by boost::instrusive_ptr to do automatic ref-counting
inline void intrusive_ptr_add_ref(Object* object) {
    ++object->ref_count;
}

inline void intrusive_ptr_release(Object* object) {
    if (--object->ref_count == 0) {
        delete object;
    }
}

/**
 * This class allows the creation of Handles, which are references to objects that can be tested
 * for validity and looked up. Here they are used to pass references to kernel objects to/from the
 * emulated process. it has been designed so that it follows the same handle format and has
 * approximately the same restrictions as the handle manager in the CTR-OS.
 *
 * Handles contain two sub-fields: a slot index (bits 31:15) and a generation value (bits 14:0).
 * The slot index is used to index into the arrays in this class to access the data corresponding
 * to the Handle.
 *
 * To prevent accidental use of a freed Handle whose slot has already been reused, a global counter
 * is kept and incremented every time a Handle is created. This is the Handle's "generation". The
 * value of the counter is stored into the Handle as well as in the handle table (in the
 * "generations" array). When looking up a handle, the Handle's generation must match with the
 * value stored on the class, otherwise the Handle is considered invalid.
 *
 * To find free slots when allocating a Handle without needing to scan the entire object array, the
 * generations field of unallocated slots is re-purposed as a linked list of indices to free slots.
 * When a Handle is created, an index is popped off the list and used for the new Handle. When it
 * is destroyed, it is again pushed onto the list to be re-used by the next allocation. It is
 * likely that this allocation strategy differs from the one used in CTR-OS, but this hasn't been
 * verified and isn't likely to cause any problems.
 */
class HandleTable final : NonCopyable {
public:
    HandleTable();

    /**
     * Allocates a handle for the given object.
     * @return The created Handle or one of the following errors:
     *           - `ERR_OUT_OF_HANDLES`: the maximum number of handles has been exceeded.
     */
    ResultVal<Handle> Create(Object* obj);

    /**
     * Returns a new handle that points to the same object as the passed in handle.
     * @return The duplicated Handle or one of the following errors:
     *           - `ERR_INVALID_HANDLE`: an invalid handle was passed in.
     *           - Any errors returned by `Create()`.
     */
    ResultVal<Handle> Duplicate(Handle handle);

    /**
     * Closes a handle, removing it from the table and decreasing the object's ref-count.
     * @return `RESULT_SUCCESS` or one of the following errors:
     *           - `ERR_INVALID_HANDLE`: an invalid handle was passed in.
     */
    ResultCode Close(Handle handle);

    /// Checks if a handle is valid and points to an existing object.
    bool IsValid(Handle handle) const;

    /**
     * Looks up a handle.
     * @returns Pointer to the looked-up object, or `nullptr` if the handle is not valid.
     */
    Object* GetGeneric(Handle handle) const;

    /**
     * Looks up a handle while verifying its type.
     * @returns Pointer to the looked-up object, or `nullptr` if the handle is not valid or its
     *          type differs from the handle type `T::HANDLE_TYPE`.
     */
    template <class T>
    T* Get(Handle handle) const {
        Object* object = GetGeneric(handle);
        if (object != nullptr && object->GetHandleType() == T::HANDLE_TYPE) {
            return static_cast<T*>(object);
        }
        return nullptr;
    }

    /// Closes all handles held in this table.
    void Clear();

private:
    /**
     * This is the maximum limit of handles allowed per process in CTR-OS. It can be further
     * reduced by ExHeader values, but this is not emulated here.
     */
    static const size_t MAX_COUNT = 4096;

    static size_t GetSlot(Handle handle)    { return handle >> 15; }
    static u16 GetGeneration(Handle handle) { return handle & 0x7FFF; }

    /// Stores the Object referenced by the handle or null if the slot is empty.
    std::array<Object*, MAX_COUNT> objects;

    /**
     * The value of `next_generation` when the handle was created, used to check for validity. For
     * empty slots, contains the index of the next free slot in the list.
     */
    std::array<u16, MAX_COUNT> generations;

    /**
     * Global counter of the number of created handles. Stored in `generations` when a handle is
     * created, and wraps around to 1 when it hits 0x8000.
     */
    u16 next_generation;

    /// Head of the free slots linked list.
    u16 next_free_slot;
};

extern HandleTable g_handle_table;
extern Handle g_main_thread;

/// The ID code of the currently running game
/// TODO(Subv): This variable should not be here, 
/// we need a way to store information about the currently loaded application 
/// for later query during runtime, maybe using the LDR service?
extern u64 g_program_id;

/// Initialize the kernel
void Init();

/// Shutdown the kernel
void Shutdown();

/**
 * Loads executable stored at specified address
 * @entry_point Entry point in memory of loaded executable
 * @return True on success, otherwise false
 */
bool LoadExec(u32 entry_point);

} // namespace
