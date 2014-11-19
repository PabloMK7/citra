// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include "common/common.h"

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
    Service         = 2,
    Event           = 3,
    Mutex           = 4,
    SharedMemory    = 5,
    Redirection     = 6,
    Thread          = 7,
    Process         = 8,
    AddressArbiter  = 9,
    File            = 10,
    Semaphore       = 11,
    Archive         = 12,
    Directory       = 13,
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
     * Synchronize kernel object
     * @param wait Boolean wait set if current thread should wait as a result of sync operation
     * @return Result of operation, 0 on success, otherwise error code
     */
    virtual Result SyncRequest(bool* wait) {
        ERROR_LOG(KERNEL, "(UNIMPLEMENTED)");
        return -1;
    }

    /**
     * Wait for kernel object to synchronize
     * @param wait Boolean wait set if current thread should wait as a result of sync operation
     * @return Result of operation, 0 on success, otherwise error code
     */
    virtual Result WaitSynchronization(bool* wait) = 0;
};

class ObjectPool : NonCopyable {
public:
    ObjectPool();
    ~ObjectPool() {}

    // Allocates a handle within the range and inserts the object into the map.
    Handle Create(Object* obj, int range_bottom=INITIAL_NEXT_ID, int range_top=0x7FFFFFFF);

    static Object* CreateByIDType(int type);

    template <class T>
    u32 Destroy(Handle handle) {
        u32 error;
        if (Get<T>(handle, error)) {
            occupied[handle - HANDLE_OFFSET] = false;
            delete pool[handle - HANDLE_OFFSET];
        }
        return error;
    }

    bool IsValid(Handle handle);

    template <class T>
    T* Get(Handle handle, u32& outError) {
        if (handle < HANDLE_OFFSET || handle >= HANDLE_OFFSET + MAX_COUNT || !occupied[handle - HANDLE_OFFSET]) {
            // Tekken 6 spams 0x80020001 gets wrong with no ill effects, also on the real PSP
            if (handle != 0 && (u32)handle != 0x80020001) {
                WARN_LOG(KERNEL, "Kernel: Bad object handle %i (%08x)", handle, handle);
            }
            outError = 0;//T::GetMissingErrorCode();
            return 0;
        } else {
            // Previously we had a dynamic_cast here, but since RTTI was disabled traditionally,
            // it just acted as a static case and everything worked. This means that we will never
            // see the Wrong type object error below, but we'll just have to live with that danger.
            T* t = static_cast<T*>(pool[handle - HANDLE_OFFSET]);
            if (t == 0 || t->GetHandleType() != T::GetStaticHandleType()) {
                WARN_LOG(KERNEL, "Kernel: Wrong object type for %i (%08x)", handle, handle);
                outError = 0;//T::GetMissingErrorCode();
                return 0;
            }
            outError = 0;//SCE_KERNEL_ERROR_OK;
            return t;
        }
    }

    // ONLY use this when you know the handle is valid.
    template <class T>
    T *GetFast(Handle handle) {
        const Handle realHandle = handle - HANDLE_OFFSET;
        _dbg_assert_(KERNEL, realHandle >= 0 && realHandle < MAX_COUNT && occupied[realHandle]);
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
            ERROR_LOG(KERNEL, "Kernel: Bad object handle %i (%08x)", handle, handle);
            return false;
        }
        Object* t = pool[handle - HANDLE_OFFSET];
        *type = t->GetHandleType();
        return true;
    }

    Object* &operator [](Handle handle);
    void List();
    void Clear();
    int GetCount();

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
