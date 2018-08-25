// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <utility>

#include <boost/smart_ptr/intrusive_ptr.hpp>

#include "common/common_types.h"

namespace Kernel {

using Handle = u32;

enum class HandleType : u32 {
    Unknown,
    Event,
    Mutex,
    SharedMemory,
    Thread,
    Process,
    AddressArbiter,
    Semaphore,
    Timer,
    ResourceLimit,
    CodeSet,
    ClientPort,
    ServerPort,
    ClientSession,
    ServerSession,
};

enum {
    DEFAULT_STACK_SIZE = 0x4000,
};

enum class ResetType {
    OneShot,
    Sticky,
    Pulse,
};

class Object : NonCopyable {
public:
    virtual ~Object();

    /// Returns a unique identifier for the object. For debugging purposes only.
    unsigned int GetObjectId() const {
        return object_id;
    }

    virtual std::string GetTypeName() const {
        return "[BAD KERNEL OBJECT TYPE]";
    }
    virtual std::string GetName() const {
        return "[UNKNOWN KERNEL OBJECT]";
    }
    virtual HandleType GetHandleType() const = 0;

    /**
     * Check if a thread can wait on the object
     * @return True if a thread can wait on the object, otherwise false
     */
    bool IsWaitable() const;

public:
    static unsigned int next_object_id;

private:
    friend void intrusive_ptr_add_ref(Object*);
    friend void intrusive_ptr_release(Object*);

    unsigned int ref_count = 0;
    unsigned int object_id = next_object_id++;
};

// Special functions used by boost::instrusive_ptr to do automatic ref-counting
inline void intrusive_ptr_add_ref(Object* object) {
    ++object->ref_count;
}

inline void intrusive_ptr_release(Object* object) {
    if (--object->ref_count == 0) {
        delete object;
    }
}

template <typename T>
using SharedPtr = boost::intrusive_ptr<T>;

/**
 * Attempts to downcast the given Object pointer to a pointer to T.
 * @return Derived pointer to the object, or `nullptr` if `object` isn't of type T.
 */
template <typename T>
inline SharedPtr<T> DynamicObjectCast(SharedPtr<Object> object) {
    if (object != nullptr && object->GetHandleType() == T::HANDLE_TYPE) {
        return boost::static_pointer_cast<T>(std::move(object));
    }
    return nullptr;
}

} // namespace Kernel
