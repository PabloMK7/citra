// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include "common/common_types.h"

namespace Kernel {

class AddressArbiter;
class Event;
class Mutex;

enum class ResetType {
    OneShot,
    Sticky,
    Pulse,
};

template <typename T>
using SharedPtr = boost::intrusive_ptr<T>;

class KernelSystem {
public:
    explicit KernelSystem(u32 system_mode);
    ~KernelSystem();

    /**
     * Creates an address arbiter.
     *
     * @param name Optional name used for debugging.
     * @returns The created AddressArbiter.
     */
    SharedPtr<AddressArbiter> CreateAddressArbiter(std::string name = "Unknown");

    /**
     * Creates an event
     * @param reset_type ResetType describing how to create event
     * @param name Optional name of event
     */
    SharedPtr<Event> CreateEvent(ResetType reset_type, std::string name = "Unknown");

    /**
     * Creates a mutex.
     * @param initial_locked Specifies if the mutex should be locked initially
     * @param name Optional name of mutex
     * @return Pointer to new Mutex object
     */
    SharedPtr<Mutex> CreateMutex(bool initial_locked, std::string name = "Unknown");
};

} // namespace Kernel
