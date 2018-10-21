// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included..

#pragma once

#include <atomic>
#include <memory>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/swap.h"
#include "core/frontend/input.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/service/service.h"

namespace Kernel {
class Event;
class SharedMemory;
} // namespace Kernel

namespace CoreTiming {
struct EventType;
};

namespace Service::IR {

union PadState {
    u32_le hex{};

    BitField<14, 1, u32_le> zl;
    BitField<15, 1, u32_le> zr;

    BitField<24, 1, u32_le> c_stick_right;
    BitField<25, 1, u32_le> c_stick_left;
    BitField<26, 1, u32_le> c_stick_up;
    BitField<27, 1, u32_le> c_stick_down;
};

/// Interface to "ir:rst" service
class IR_RST final : public ServiceFramework<IR_RST> {
public:
    explicit IR_RST(Core::System& system);
    ~IR_RST();
    void ReloadInputDevices();

private:
    /**
     * GetHandles service function
     *  No input
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Handle translation descriptor
     *      3 : Shared memory handle
     *      4 : Event handle
     */
    void GetHandles(Kernel::HLERequestContext& ctx);

    /**
     * Initialize service function
     *  Inputs:
     *      1 : pad state update period in ms
     *      2 : bool output raw c-stick data
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void Initialize(Kernel::HLERequestContext& ctx);

    /**
     * Shutdown service function
     *  No input
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void Shutdown(Kernel::HLERequestContext& ctx);

    void LoadInputDevices();
    void UnloadInputDevices();
    void UpdateCallback(u64 userdata, s64 cycles_late);

    Kernel::SharedPtr<Kernel::Event> update_event;
    Kernel::SharedPtr<Kernel::SharedMemory> shared_memory;
    u32 next_pad_index{0};
    CoreTiming::EventType* update_callback_id;
    std::unique_ptr<Input::ButtonDevice> zl_button;
    std::unique_ptr<Input::ButtonDevice> zr_button;
    std::unique_ptr<Input::AnalogDevice> c_stick;
    std::atomic<bool> is_device_reload_pending{false};
    bool raw_c_stick{false};
    int update_period{0};
};

} // namespace Service::IR
