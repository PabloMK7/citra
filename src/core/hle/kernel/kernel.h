// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include "common/common_types.h"

typedef u32 UID;

enum KernelIDType {
    KERNEL_ID_TYPE_THREAD       = 1,
    KERNEL_ID_TYPE_SEMAPHORE    = 2,
    KERNEL_ID_TYPE_MUTEX        = 3,
    KERNEL_ID_TYPE_EVENT        = 4,
};

#define KERNELOBJECT_MAX_NAME_LENGTH 31

class KernelObjectPool;

class KernelObject {
    friend class KernelObjectPool;
    u32 uid;
public:
    virtual ~KernelObject() {}
    UID GetUID() const { return uid; }
    virtual const char *GetTypeName() { return "[BAD KERNEL OBJECT TYPE]"; }
    virtual const char *GetName() { return "[UNKNOWN KERNEL OBJECT]"; }
    virtual KernelIDType GetIDType() const = 0;
    //virtual void GetQuickInfo(char *ptr, int size);
};

class KernelObjectPool {
public:
    KernelObjectPool();
    ~KernelObjectPool() {}

    // Allocates a UID within the range and inserts the object into the map.
    UID Create(KernelObject *obj, int range_bottom=INITIAL_NEXT_ID, int range_top=0x7FFFFFFF);

    static KernelObject *CreateByIDType(int type);

    template <class T>
    u32 Destroy(UID handle) {
        u32 error;
        if (Get<T>(handle, error)) {
            occupied[handle - HANDLE_OFFSET] = false;
            delete pool[handle - HANDLE_OFFSET];
        }
        return error;
    };

    bool IsValid(UID handle);

    template <class T>
    T* Get(UID handle, u32& outError) {
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
            if (t == 0 || t->GetIDType() != T::GetStaticIDType()) {
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
    T *GetFast(UID handle) {
        const UID realHandle = handle - HANDLE_OFFSET;
        _dbg_assert_(KERNEL, realHandle >= 0 && realHandle < MAX_COUNT && occupied[realHandle]);
        return static_cast<T *>(pool[realHandle]);
    }

    template <class T, typename ArgT>
    void Iterate(bool func(T *, ArgT), ArgT arg) {
        int type = T::GetStaticIDType();
        for (int i = 0; i < MAX_COUNT; i++)
        {
            if (!occupied[i])
                continue;
            T *t = static_cast<T *>(pool[i]);
            if (t->GetIDType() == type) {
                if (!func(t, arg))
                    break;
            }
        }
    }

    bool GetIDType(UID handle, int *type) const {
        if ((handle < HANDLE_OFFSET) || (handle >= HANDLE_OFFSET + MAX_COUNT) || 
            !occupied[handle - HANDLE_OFFSET]) {
            ERROR_LOG(KERNEL, "Kernel: Bad object handle %i (%08x)", handle, handle);
            return false;
        }
        KernelObject *t = pool[handle - HANDLE_OFFSET];
        *type = t->GetIDType();
        return true;
    }

    KernelObject *&operator [](UID handle);
    void List();
    void Clear();
    int GetCount();

private:
    enum {
        MAX_COUNT       = 0x1000,
        HANDLE_OFFSET   = 0x100,
        INITIAL_NEXT_ID = 0x10,
    };
    KernelObject *pool[MAX_COUNT];
    bool occupied[MAX_COUNT];
    int next_id;
};

extern KernelObjectPool g_kernel_objects;

bool __KernelLoadExec(u32 entry_point);
