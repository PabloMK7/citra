// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include "common/bit_field.h"
#include "core/core_timing.h"
#include "core/frontend/input.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/ir/ir.h"
#include "core/hle/service/ir/ir_rst.h"
#include "core/settings.h"

namespace Service {
namespace IR {

union PadState {
    u32_le hex{};

    BitField<14, 1, u32_le> zl;
    BitField<15, 1, u32_le> zr;

    BitField<24, 1, u32_le> c_stick_right;
    BitField<25, 1, u32_le> c_stick_left;
    BitField<26, 1, u32_le> c_stick_up;
    BitField<27, 1, u32_le> c_stick_down;
};

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

static Kernel::SharedPtr<Kernel::Event> update_event;
static Kernel::SharedPtr<Kernel::SharedMemory> shared_memory;
static u32 next_pad_index;
static int update_callback_id;
static std::unique_ptr<Input::ButtonDevice> zl_button;
static std::unique_ptr<Input::ButtonDevice> zr_button;
static std::unique_ptr<Input::AnalogDevice> c_stick;
static std::atomic<bool> is_device_reload_pending;
static bool raw_c_stick;
static int update_period;

static void LoadInputDevices() {
    zl_button = Input::CreateDevice<Input::ButtonDevice>(
        Settings::values.buttons[Settings::NativeButton::ZL]);
    zr_button = Input::CreateDevice<Input::ButtonDevice>(
        Settings::values.buttons[Settings::NativeButton::ZR]);
    c_stick = Input::CreateDevice<Input::AnalogDevice>(
        Settings::values.analogs[Settings::NativeAnalog::CStick]);
}

static void UnloadInputDevices() {
    zl_button = nullptr;
    zr_button = nullptr;
    c_stick = nullptr;
}

static void UpdateCallback(u64 userdata, int cycles_late) {
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
    const s16 c_stick_x = static_cast<s16>(c_stick_x_f * MAX_CSTICK_RADIUS);
    const s16 c_stick_y = static_cast<s16>(c_stick_y_f * MAX_CSTICK_RADIUS);

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

/**
 * IR::GetHandles service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Translate header, used by the ARM11-kernel
 *      3 : Shared memory handle
 *      4 : Event handle
 */
static void GetHandles(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x01, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 3);
    rb.Push(RESULT_SUCCESS);
    rb.PushMoveHandles(Kernel::g_handle_table.Create(Service::IR::shared_memory).Unwrap(),
                       Kernel::g_handle_table.Create(Service::IR::update_event).Unwrap());
}

/**
 * IR::Initialize service function
 *  Inputs:
 *      1 : pad state update period in ms
 *      2 : bool output raw c-stick data
 */
static void Initialize(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x02, 2, 0);
    update_period = static_cast<int>(rp.Pop<u32>());
    raw_c_stick = rp.Pop<bool>();

    if (raw_c_stick)
        LOG_ERROR(Service_IR, "raw C-stick data is not implemented!");

    next_pad_index = 0;
    is_device_reload_pending.store(true);
    CoreTiming::ScheduleEvent(msToCycles(update_period), update_callback_id);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_IR, "called. update_period=%d, raw_c_stick=%d", update_period, raw_c_stick);
}

static void Shutdown(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x03, 1, 0);

    CoreTiming::UnscheduleEvent(update_callback_id, 0);
    UnloadInputDevices();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_IR, "called");
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010000, GetHandles, "GetHandles"},
    {0x00020080, Initialize, "Initialize"},
    {0x00030000, Shutdown, "Shutdown"},
    {0x00090000, nullptr, "WriteToTwoFields"},
};

IR_RST_Interface::IR_RST_Interface() {
    Register(FunctionTable);
}

void InitRST() {
    using namespace Kernel;
    // Note: these two kernel objects are even available before Initialize service function is
    // called.
    shared_memory =
        SharedMemory::Create(nullptr, 0x1000, MemoryPermission::ReadWrite, MemoryPermission::Read,
                             0, MemoryRegion::BASE, "IRRST:SharedMemory");
    update_event = Event::Create(ResetType::OneShot, "IRRST:UpdateEvent");

    update_callback_id = CoreTiming::RegisterEvent("IRRST:UpdateCallBack", UpdateCallback);
}

void ShutdownRST() {
    shared_memory = nullptr;
    update_event = nullptr;
    UnloadInputDevices();
}

void ReloadInputDevicesRST() {
    is_device_reload_pending.store(true);
}

} // namespace IR
} // namespace Service
