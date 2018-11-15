// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/wait_object.h"

namespace Kernel {

class Event final : public WaitObject {
public:
    std::string GetTypeName() const override {
        return "Event";
    }
    std::string GetName() const override {
        return name;
    }
    void SetName(const std::string& name) {
        this->name = name;
    }

    static const HandleType HANDLE_TYPE = HandleType::Event;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    ResetType GetResetType() const {
        return reset_type;
    }

    bool ShouldWait(Thread* thread) const override;
    void Acquire(Thread* thread) override;

    void WakeupAllWaitingThreads() override;

    void Signal();
    void Clear();

private:
    explicit Event(KernelSystem& kernel);
    ~Event() override;

    ResetType reset_type; ///< Current ResetType

    bool signaled;    ///< Whether the event has already been signaled
    std::string name; ///< Name of event (optional)

    friend class KernelSystem;
};

} // namespace Kernel
