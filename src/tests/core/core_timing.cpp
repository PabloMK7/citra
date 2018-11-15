// Copyright 2016 Dolphin Emulator Project / 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <catch2/catch.hpp>

#include <array>
#include <bitset>
#include <string>
#include "common/file_util.h"
#include "core/core.h"
#include "core/core_timing.h"

// Numbers are chosen randomly to make sure the correct one is given.
static constexpr std::array<u64, 5> CB_IDS{{42, 144, 93, 1026, UINT64_C(0xFFFF7FFFF7FFFF)}};
static constexpr int MAX_SLICE_LENGTH = 20000; // Copied from CoreTiming internals

static std::bitset<CB_IDS.size()> callbacks_ran_flags;
static u64 expected_callback = 0;
static s64 lateness = 0;

template <unsigned int IDX>
void CallbackTemplate(u64 userdata, s64 cycles_late) {
    static_assert(IDX < CB_IDS.size(), "IDX out of range");
    callbacks_ran_flags.set(IDX);
    REQUIRE(CB_IDS[IDX] == userdata);
    REQUIRE(CB_IDS[IDX] == expected_callback);
    REQUIRE(lateness == cycles_late);
}

static void AdvanceAndCheck(Core::Timing& timing, u32 idx, int downcount, int expected_lateness = 0,
                            int cpu_downcount = 0) {
    callbacks_ran_flags = 0;
    expected_callback = CB_IDS[idx];
    lateness = expected_lateness;

    timing.AddTicks(timing.GetDowncount() -
                    cpu_downcount); // Pretend we executed X cycles of instructions.
    timing.Advance();

    REQUIRE(decltype(callbacks_ran_flags)().set(idx) == callbacks_ran_flags);
    REQUIRE(downcount == timing.GetDowncount());
}

TEST_CASE("CoreTiming[BasicOrder]", "[core]") {
    Core::Timing timing;

    Core::TimingEventType* cb_a = timing.RegisterEvent("callbackA", CallbackTemplate<0>);
    Core::TimingEventType* cb_b = timing.RegisterEvent("callbackB", CallbackTemplate<1>);
    Core::TimingEventType* cb_c = timing.RegisterEvent("callbackC", CallbackTemplate<2>);
    Core::TimingEventType* cb_d = timing.RegisterEvent("callbackD", CallbackTemplate<3>);
    Core::TimingEventType* cb_e = timing.RegisterEvent("callbackE", CallbackTemplate<4>);

    // Enter slice 0
    timing.Advance();

    // D -> B -> C -> A -> E
    timing.ScheduleEvent(1000, cb_a, CB_IDS[0]);
    REQUIRE(1000 == timing.GetDowncount());
    timing.ScheduleEvent(500, cb_b, CB_IDS[1]);
    REQUIRE(500 == timing.GetDowncount());
    timing.ScheduleEvent(800, cb_c, CB_IDS[2]);
    REQUIRE(500 == timing.GetDowncount());
    timing.ScheduleEvent(100, cb_d, CB_IDS[3]);
    REQUIRE(100 == timing.GetDowncount());
    timing.ScheduleEvent(1200, cb_e, CB_IDS[4]);
    REQUIRE(100 == timing.GetDowncount());

    AdvanceAndCheck(timing, 3, 400);
    AdvanceAndCheck(timing, 1, 300);
    AdvanceAndCheck(timing, 2, 200);
    AdvanceAndCheck(timing, 0, 200);
    AdvanceAndCheck(timing, 4, MAX_SLICE_LENGTH);
}

TEST_CASE("CoreTiming[Threadsave]", "[core]") {
    Core::Timing timing;

    Core::TimingEventType* cb_a = timing.RegisterEvent("callbackA", CallbackTemplate<0>);
    Core::TimingEventType* cb_b = timing.RegisterEvent("callbackB", CallbackTemplate<1>);
    Core::TimingEventType* cb_c = timing.RegisterEvent("callbackC", CallbackTemplate<2>);
    Core::TimingEventType* cb_d = timing.RegisterEvent("callbackD", CallbackTemplate<3>);
    Core::TimingEventType* cb_e = timing.RegisterEvent("callbackE", CallbackTemplate<4>);

    // Enter slice 0
    timing.Advance();

    // D -> B -> C -> A -> E
    timing.ScheduleEventThreadsafe(1000, cb_a, CB_IDS[0]);
    // Manually force since ScheduleEventThreadsafe doesn't call it
    timing.ForceExceptionCheck(1000);
    REQUIRE(1000 == timing.GetDowncount());
    timing.ScheduleEventThreadsafe(500, cb_b, CB_IDS[1]);
    // Manually force since ScheduleEventThreadsafe doesn't call it
    timing.ForceExceptionCheck(500);
    REQUIRE(500 == timing.GetDowncount());
    timing.ScheduleEventThreadsafe(800, cb_c, CB_IDS[2]);
    // Manually force since ScheduleEventThreadsafe doesn't call it
    timing.ForceExceptionCheck(800);
    REQUIRE(500 == timing.GetDowncount());
    timing.ScheduleEventThreadsafe(100, cb_d, CB_IDS[3]);
    // Manually force since ScheduleEventThreadsafe doesn't call it
    timing.ForceExceptionCheck(100);
    REQUIRE(100 == timing.GetDowncount());
    timing.ScheduleEventThreadsafe(1200, cb_e, CB_IDS[4]);
    // Manually force since ScheduleEventThreadsafe doesn't call it
    timing.ForceExceptionCheck(1200);
    REQUIRE(100 == timing.GetDowncount());

    AdvanceAndCheck(timing, 3, 400);
    AdvanceAndCheck(timing, 1, 300);
    AdvanceAndCheck(timing, 2, 200);
    AdvanceAndCheck(timing, 0, 200);
    AdvanceAndCheck(timing, 4, MAX_SLICE_LENGTH);
}

namespace SharedSlotTest {
static unsigned int counter = 0;

template <unsigned int ID>
void FifoCallback(u64 userdata, s64 cycles_late) {
    static_assert(ID < CB_IDS.size(), "ID out of range");
    callbacks_ran_flags.set(ID);
    REQUIRE(CB_IDS[ID] == userdata);
    REQUIRE(ID == counter);
    REQUIRE(lateness == cycles_late);
    ++counter;
}
} // namespace SharedSlotTest

TEST_CASE("CoreTiming[SharedSlot]", "[core]") {
    using namespace SharedSlotTest;

    Core::Timing timing;

    Core::TimingEventType* cb_a = timing.RegisterEvent("callbackA", FifoCallback<0>);
    Core::TimingEventType* cb_b = timing.RegisterEvent("callbackB", FifoCallback<1>);
    Core::TimingEventType* cb_c = timing.RegisterEvent("callbackC", FifoCallback<2>);
    Core::TimingEventType* cb_d = timing.RegisterEvent("callbackD", FifoCallback<3>);
    Core::TimingEventType* cb_e = timing.RegisterEvent("callbackE", FifoCallback<4>);

    timing.ScheduleEvent(1000, cb_a, CB_IDS[0]);
    timing.ScheduleEvent(1000, cb_b, CB_IDS[1]);
    timing.ScheduleEvent(1000, cb_c, CB_IDS[2]);
    timing.ScheduleEvent(1000, cb_d, CB_IDS[3]);
    timing.ScheduleEvent(1000, cb_e, CB_IDS[4]);

    // Enter slice 0
    timing.Advance();
    REQUIRE(1000 == timing.GetDowncount());

    callbacks_ran_flags = 0;
    counter = 0;
    lateness = 0;
    timing.AddTicks(timing.GetDowncount());
    timing.Advance();
    REQUIRE(MAX_SLICE_LENGTH == timing.GetDowncount());
    REQUIRE(0x1FULL == callbacks_ran_flags.to_ullong());
}

TEST_CASE("CoreTiming[PredictableLateness]", "[core]") {
    Core::Timing timing;

    Core::TimingEventType* cb_a = timing.RegisterEvent("callbackA", CallbackTemplate<0>);
    Core::TimingEventType* cb_b = timing.RegisterEvent("callbackB", CallbackTemplate<1>);

    // Enter slice 0
    timing.Advance();

    timing.ScheduleEvent(100, cb_a, CB_IDS[0]);
    timing.ScheduleEvent(200, cb_b, CB_IDS[1]);

    AdvanceAndCheck(timing, 0, 90, 10, -10); // (100 - 10)
    AdvanceAndCheck(timing, 1, MAX_SLICE_LENGTH, 50, -50);
}

namespace ChainSchedulingTest {
static int reschedules = 0;

static void RescheduleCallback(Core::Timing& timing, u64 userdata, s64 cycles_late) {
    --reschedules;
    REQUIRE(reschedules >= 0);
    REQUIRE(lateness == cycles_late);

    if (reschedules > 0)
        timing.ScheduleEvent(1000, reinterpret_cast<Core::TimingEventType*>(userdata), userdata);
}
} // namespace ChainSchedulingTest

TEST_CASE("CoreTiming[ChainScheduling]", "[core]") {
    using namespace ChainSchedulingTest;

    Core::Timing timing;

    Core::TimingEventType* cb_a = timing.RegisterEvent("callbackA", CallbackTemplate<0>);
    Core::TimingEventType* cb_b = timing.RegisterEvent("callbackB", CallbackTemplate<1>);
    Core::TimingEventType* cb_c = timing.RegisterEvent("callbackC", CallbackTemplate<2>);
    Core::TimingEventType* cb_rs =
        timing.RegisterEvent("callbackReschedule", [&timing](u64 userdata, s64 cycles_late) {
            RescheduleCallback(timing, userdata, cycles_late);
        });

    // Enter slice 0
    timing.Advance();

    timing.ScheduleEvent(800, cb_a, CB_IDS[0]);
    timing.ScheduleEvent(1000, cb_b, CB_IDS[1]);
    timing.ScheduleEvent(2200, cb_c, CB_IDS[2]);
    timing.ScheduleEvent(1000, cb_rs, reinterpret_cast<u64>(cb_rs));
    REQUIRE(800 == timing.GetDowncount());

    reschedules = 3;
    AdvanceAndCheck(timing, 0, 200);  // cb_a
    AdvanceAndCheck(timing, 1, 1000); // cb_b, cb_rs
    REQUIRE(2 == reschedules);

    timing.AddTicks(timing.GetDowncount());
    timing.Advance(); // cb_rs
    REQUIRE(1 == reschedules);
    REQUIRE(200 == timing.GetDowncount());

    AdvanceAndCheck(timing, 2, 800); // cb_c

    timing.AddTicks(timing.GetDowncount());
    timing.Advance(); // cb_rs
    REQUIRE(0 == reschedules);
    REQUIRE(MAX_SLICE_LENGTH == timing.GetDowncount());
}
