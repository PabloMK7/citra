// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include "common/common.h"
#include "core/hle/result.h"

typedef u32 Handle;
typedef s32 Result;

namespace Kernel {

enum KernelHandle {
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

class ObjectPool;

class Object : NonCopyable {
    friend class ObjectPool;
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
};

class ObjectPool : NonCopyable {
public:
    ObjectPool();
    ~ObjectPool() {}

    // Allocates a handle within the range and inserts the object into the map.
    Handle Create(Object* obj, int range_bottom=INITIAL_NEXT_ID, int range_top=0x7FFFFFFF);

    static Object* CreateByIDType(int type);

    template <class T>
    void Destroy(Handle handle) {
        if (Get<T>(handle)) {
            occupied[handle - HANDLE_OFFSET] = false;
            delete pool[handle - HANDLE_OFFSET];
        }
    }

    bool IsValid(Handle handle) const;

    template <class T>
    T* Get(Handle handle) {
        if (handle < HANDLE_OFFSET || handle >= HANDLE_OFFSET + MAX_COUNT || !occupied[handle - HANDLE_OFFSET]) {
            if (handle != 0) {
                LOG_ERROR(Kernel, "Bad object handle %08x", handle);
            }
            return nullptr;
        } else {
            Object* t = pool[handle - HANDLE_OFFSET];
            if (t->GetHandleType() != T::GetStaticHandleType()) {
                LOG_ERROR(Kernel, "Wrong object type for %08x", handle);
                return nullptr;
            }
            return static_cast<T*>(t);
        }
    }

    // ONLY use this when you know the handle is valid.
    template <class T>
    T *GetFast(Handle handle) {
        const Handle realHandle = handle - HANDLE_OFFSET;
        _dbg_assert_(Kernel, realHandle >= 0 && realHandle < MAX_COUNT && occupied[realHandle]);
        return static_cast<T*>(pool[realHandle]);
    }

    template <class T, typename ArgT>
    void Iterate(bool func(T*, ArgT), ArgT arg) {
        int type = T::GetStaticIDType();
        for (int i = 0; i < MAX_COUNT; i++)
        {
            if (!occupied[i])
                continue;
            T* t = static_cast<T*>(pool[i]);
            if (t->GetIDType() == type) {
                if (!func(t, arg))
                    break;
            }
        }
    }

    bool GetIDType(Handle handle, HandleType* type) const {
        if ((handle < HANDLE_OFFSET) || (handle >= HANDLE_OFFSET + MAX_COUNT) ||
                !occupied[handle - HANDLE_OFFSET]) {
            LOG_ERROR(Kernel, "Bad object handle %08X", handle);
            return false;
        }
        Object* t = pool[handle - HANDLE_OFFSET];
        *type = t->GetHandleType();
        return true;
    }

    Object* &operator [](Handle handle);
    void List();
    void Clear();
    int GetCount() const;

private:

    enum {
        MAX_COUNT       = 0x1000,
        HANDLE_OFFSET   = 0x100,
        INITIAL_NEXT_ID = 0x10,
    };

    std::array<Object*, MAX_COUNT> pool;
    std::array<bool, MAX_COUNT> occupied;
    int next_id;
};

extern ObjectPool g_object_pool;
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
