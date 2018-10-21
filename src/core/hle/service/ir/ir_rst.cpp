// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/ir/ir_rst.h"
#include "core/movie.h"
#include "core/settings.h"

namespace Service::IR {

struct PadDataEntry {
    PadState current_state;
    PadState delta_additions;
    PadState delta_removals;

    s16_le c_stick_x;
    s16_le c_stick_y;
};

struct SharedMem {
    u64_le index_reset_ticks;          ///< CPU tick count for when HID module updated entry index 0
    u64_le index_reset_ticks_previous; ///< Previous `index_reset_ticks`
    u32_le index;
    INSERT_PADDING_WORDS(1);
    std::array<PadDataEntry, 8> entries; ///< Last 8 pad entries
};

static_assert(sizeof(SharedMem) == 0x98, "SharedMem has wrong size!");

void IR_RST::LoadInputDevices() {
    zl_button = Input::CreateDevice<Input::ButtonDevice>(
        Settings::values.buttons[Settings::NativeButton::ZL]);
    zr_button = Input::CreateDevice<Input::ButtonDevice>(
        Settings::values.buttons[Settings::NativeButton::ZR]);
    c_stick = Input::CreateDevice<Input::AnalogDevice>(
        Settings::values.analogs[Settings::NativeAnalog::CStick]);
}

void IR_RST::UnloadInputDevices() {
    zl_button = nullptr;
    zr_button = nullptr;
    c_stick = nullptr;
}

void IR_RST::UpdateCallback(u64 userdata, s64 cycles_late) {
    SharedMem* mem = reinterpret_cast<SharedMem*>(shared_memory->GetPointer());

    if (is_device_reload_pending.exchange(false))
        LoadInputDevices();

    PadState state;
    state.zl.Assign(zl_button->GetStatus());
    state.zr.Assign(zr_button->GetStatus());

    // Get current c-stick position and update c-stick direction
    float c_stick_x_f, c_stick_y_f;
    std::tie(c_stick_x_f, c_stick_y_f) = c_stick->GetStatus();
    constexpr int MAX_CSTICK_RADIUS = 0x9C; // Max value for a c-stick radius
    s16 c_stick_x = static_cast<s16>(c_stick_x_f * MAX_CSTICK_RADIUS);
    s16 c_stick_y = static_cast<s16>(c_stick_y_f * MAX_CSTICK_RADIUS);

    Core::Movie::GetInstance().HandleIrRst(state, c_stick_x, c_stick_y);

    if (!raw_c_stick) {
        const HID::DirectionState direction = HID::GetStickDirectionState(c_stick_x, c_stick_y);
        state.c_stick_up.Assign(direction.up);
        state.c_stick_down.Assign(direction.down);
        state.c_stick_left.Assign(direction.left);
        state.c_stick_right.Assign(direction.right);
    }

    // TODO (wwylele): implement raw C-stick data for raw_c_stick = true

    const u32 last_entry_index = mem->index;
    mem->index = next_pad_index;
    next_pad_index = (next_pad_index + 1) % mem->entries.size();

    // Get the previous Pad state
    PadState old_state{mem->entries[last_entry_index].current_state};

    // Compute bitmask with 1s for bits different from the old state
    PadState changed = {state.hex ^ old_state.hex};

    // Get the current Pad entry
    PadDataEntry& pad_entry = mem->entries[mem->index];

    // Update entry properties
    pad_entry.current_state.hex = state.hex;
    pad_entry.delta_additions.hex = changed.hex & state.hex;
    pad_entry.delta_removals.hex = changed.hex & old_state.hex;
    pad_entry.c_stick_x = c_stick_x;
    pad_entry.c_stick_y = c_stick_y;

    // If we just updated index 0, provide a new timestamp
    if (mem->index == 0) {
        mem->index_reset_ticks_previous = mem->index_reset_ticks;
        mem->index_reset_ticks = CoreTiming::GetTicks();
    }

    update_event->Signal();

    // Reschedule recurrent event
    CoreTiming::ScheduleEvent(msToCycles(update_period) - cycles_late, update_callback_id);
}

void IR_RST::GetHandles(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x01, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 3);
    rb.Push(RESULT_SUCCESS);
    rb.PushMoveObjects(shared_memory, update_event);
}

void IR_RST::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x02, 2, 0);
    update_period = static_cast<int>(rp.Pop<u32>());
    raw_c_stick = rp.Pop<bool>();

    if (raw_c_stick)
        LOG_ERROR(Service_IR, "raw C-stick data is not implemented!");

    next_pad_index = 0;
    is_device_reload_pending.store(true);
    CoreTiming::ScheduleEvent(msToCycles(update_period), update_callback_id);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_IR, "called. update_period={}, raw_c_stick={}", update_period, raw_c_stick);
}

void IR_RST::Shutdown(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x03, 0, 0);

    CoreTiming::UnscheduleEvent(update_callback_id, 0);
    UnloadInputDevices();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_IR, "called");
}

IR_RST::IR_RST(Core::System& system) : ServiceFramework("ir:rst", 1) {
    using namespace Kernel;
    // Note: these two kernel objects are even available before Initialize service function is
    // called.
    shared_memory = system.Kernel().CreateSharedMemory(nullptr, 0x1000, MemoryPermission::ReadWrite,
                                                       MemoryPermission::Read, 0,
                                                       MemoryRegion::BASE, "IRRST:SharedMemory");
    update_event = system.Kernel().CreateEvent(ResetType::OneShot, "IRRST:UpdateEvent");

    update_callback_id =
        CoreTiming::RegisterEvent("IRRST:UpdateCallBack", [this](u64 userdata, s64 cycles_late) {
            UpdateCallback(userdata, cycles_late);
        });

    static const FunctionInfo functions[] = {
        {0x00010000, &IR_RST::GetHandles, "GetHandles"},
        {0x00020080, &IR_RST::Initialize, "Initialize"},
        {0x00030000, &IR_RST::Shutdown, "Shutdown"},
        {0x00090000, nullptr, "WriteToTwoFields"},
    };
    RegisterHandlers(functions);
}

IR_RST::~IR_RST() = default;

void IR_RST::ReloadInputDevices() {
    is_device_reload_pending.store(true);
}

} // namespace Service::IR
