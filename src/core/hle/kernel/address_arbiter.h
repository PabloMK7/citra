// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include "common/common_types.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/result.h"

// Address arbiters are an underlying kernel synchronization object that can be created/used via
// supervisor calls (SVCs). They function as sort of a global lock. Typically, games/other CTR
// applications use them as an underlying mechanism to implement thread-safe barriers, events, and
// semaphores.

////////////////////////////////////////////////////////////////////////////////////////////////////
// Kernel namespace

namespace Kernel {

class Thread;

enum class ArbitrationType : u32 {
    Signal,
    WaitIfLessThan,
    DecrementAndWaitIfLessThan,
    WaitIfLessThanWithTimeout,
    DecrementAndWaitIfLessThanWithTimeout,
};

class AddressArbiter final : public Object, public WakeupCallback {
public:
    explicit AddressArbiter(KernelSystem& kernel);
    ~AddressArbiter() override;

    std::string GetTypeName() const override {
        return "Arbiter";
    }
    std::string GetName() const override {
        return name;
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::AddressArbiter;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    std::string name; ///< Name of address arbiter object (optional)

    ResultCode ArbitrateAddress(std::shared_ptr<Thread> thread, ArbitrationType type, VAddr address,
                                s32 value, u64 nanoseconds);

    void WakeUp(ThreadWakeupReason reason, std::shared_ptr<Thread> thread,
                std::shared_ptr<WaitObject> object);

private:
    KernelSystem& kernel;

    /// Puts the thread to wait on the specified arbitration address under this address arbiter.
    void WaitThread(std::shared_ptr<Thread> thread, VAddr wait_address);

    /// Resume all threads found to be waiting on the address under this address arbiter
    void ResumeAllThreads(VAddr address);

    /// Resume one thread found to be waiting on the address under this address arbiter and return
    /// the resumed thread.
    std::shared_ptr<Thread> ResumeHighestPriorityThread(VAddr address);

    /// Threads waiting for the address arbiter to be signaled.
    std::vector<std::shared_ptr<Thread>> waiting_threads;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int file_version) {
        ar& boost::serialization::base_object<Object>(*this);
        ar& name;
        ar& waiting_threads;
    }
};

} // namespace Kernel

BOOST_CLASS_EXPORT_KEY(Kernel::AddressArbiter)
CONSTRUCT_KERNEL_OBJECT(Kernel::AddressArbiter)
