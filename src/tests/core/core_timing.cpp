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

class ScopeInit final {
public:
    ScopeInit() {
        CoreTiming::Init();
    }
    ~ScopeInit() {
        CoreTiming::Shutdown();
    }
};

static void AdvanceAndCheck(u32 idx, int downcount, int expected_lateness = 0,
                            int cpu_downcount = 0) {
    callbacks_ran_flags = 0;
    expected_callback = CB_IDS[idx];
    lateness = expected_lateness;

    CoreTiming::AddTicks(CoreTiming::GetDowncount() -
                         cpu_downcount); // Pretend we executed X cycles of instructions.
    CoreTiming::Advance();

    REQUIRE(decltype(callbacks_ran_flags)().set(idx) == callbacks_ran_flags);
    REQUIRE(downcount == CoreTiming::GetDowncount());
}

TEST_CASE("CoreTiming[BasicOrder]", "[core]") {
    ScopeInit guard;

    CoreTiming::EventType* cb_a = CoreTiming::RegisterEvent("callbackA", CallbackTemplate<0>);
    CoreTiming::EventType* cb_b = CoreTiming::RegisterEvent("callbackB", CallbackTemplate<1>);
    CoreTiming::EventType* cb_c = CoreTiming::RegisterEvent("callbackC", CallbackTemplate<2>);
    CoreTiming::EventType* cb_d = CoreTiming::RegisterEvent("callbackD", CallbackTemplate<3>);
    CoreTiming::EventType* cb_e = CoreTiming::RegisterEvent("callbackE", CallbackTemplate<4>);

    // Enter slice 0
    CoreTiming::Advance();

    // D -> B -> C -> A -> E
    CoreTiming::ScheduleEvent(1000, cb_a, CB_IDS[0]);
    REQUIRE(1000 == CoreTiming::GetDowncount());
    CoreTiming::ScheduleEvent(500, cb_b, CB_IDS[1]);
    REQUIRE(500 == CoreTiming::GetDowncount());
    CoreTiming::ScheduleEvent(800, cb_c, CB_IDS[2]);
    REQUIRE(500 == CoreTiming::GetDowncount());
    CoreTiming::ScheduleEvent(100, cb_d, CB_IDS[3]);
    REQUIRE(100 == CoreTiming::GetDowncount());
    CoreTiming::ScheduleEvent(1200, cb_e, CB_IDS[4]);
    REQUIRE(100 == CoreTiming::GetDowncount());

    AdvanceAndCheck(3, 400);
    AdvanceAndCheck(1, 300);
    AdvanceAndCheck(2, 200);
    AdvanceAndCheck(0, 200);
    AdvanceAndCheck(4, MAX_SLICE_LENGTH);
}

TEST_CASE("CoreTiming[Threadsave]", "[core]") {
    ScopeInit guard;

    CoreTiming::EventType* cb_a = CoreTiming::RegisterEvent("callbackA", CallbackTemplate<0>);
    CoreTiming::EventType* cb_b = CoreTiming::RegisterEvent("callbackB", CallbackTemplate<1>);
    CoreTiming::EventType* cb_c = CoreTiming::RegisterEvent("callbackC", CallbackTemplate<2>);
    CoreTiming::EventType* cb_d = CoreTiming::RegisterEvent("callbackD", CallbackTemplate<3>);
    CoreTiming::EventType* cb_e = CoreTiming::RegisterEvent("callbackE", CallbackTemplate<4>);

    // Enter slice 0
    CoreTiming::Advance();

    // D -> B -> C -> A -> E
    CoreTiming::ScheduleEventThreadsafe(1000, cb_a, CB_IDS[0]);
    // Manually force since ScheduleEventThreadsafe doesn't call it
    CoreTiming::ForceExceptionCheck(1000);
    REQUIRE(1000 == CoreTiming::GetDowncount());
    CoreTiming::ScheduleEventThreadsafe(500, cb_b, CB_IDS[1]);
    // Manually force since ScheduleEventThreadsafe doesn't call it
    CoreTiming::ForceExceptionCheck(500);
    REQUIRE(500 == CoreTiming::GetDowncount());
    CoreTiming::ScheduleEventThreadsafe(800, cb_c, CB_IDS[2]);
    // Manually force since ScheduleEventThreadsafe doesn't call it
    CoreTiming::ForceExceptionCheck(800);
    REQUIRE(500 == CoreTiming::GetDowncount());
    CoreTiming::ScheduleEventThreadsafe(100, cb_d, CB_IDS[3]);
    // Manually force since ScheduleEventThreadsafe doesn't call it
    CoreTiming::ForceExceptionCheck(100);
    REQUIRE(100 == CoreTiming::GetDowncount());
    CoreTiming::ScheduleEventThreadsafe(1200, cb_e, CB_IDS[4]);
    // Manually force since ScheduleEventThreadsafe doesn't call it
    CoreTiming::ForceExceptionCheck(1200);
    REQUIRE(100 == CoreTiming::GetDowncount());

    AdvanceAndCheck(3, 400);
    AdvanceAndCheck(1, 300);
    AdvanceAndCheck(2, 200);
    AdvanceAndCheck(0, 200);
    AdvanceAndCheck(4, MAX_SLICE_LENGTH);
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

    ScopeInit guard;

    CoreTiming::EventType* cb_a = CoreTiming::RegisterEvent("callbackA", FifoCallback<0>);
    CoreTiming::EventType* cb_b = CoreTiming::RegisterEvent("callbackB", FifoCallback<1>);
    CoreTiming::EventType* cb_c = CoreTiming::RegisterEvent("callbackC", FifoCallback<2>);
    CoreTiming::EventType* cb_d = CoreTiming::RegisterEvent("callbackD", FifoCallback<3>);
    CoreTiming::EventType* cb_e = CoreTiming::RegisterEvent("callbackE", FifoCallback<4>);

    CoreTiming::ScheduleEvent(1000, cb_a, CB_IDS[0]);
    CoreTiming::ScheduleEvent(1000, cb_b, CB_IDS[1]);
    CoreTiming::ScheduleEvent(1000, cb_c, CB_IDS[2]);
    CoreTiming::ScheduleEvent(1000, cb_d, CB_IDS[3]);
    CoreTiming::ScheduleEvent(1000, cb_e, CB_IDS[4]);

    // Enter slice 0
    CoreTiming::Advance();
    REQUIRE(1000 == CoreTiming::GetDowncount());

    callbacks_ran_flags = 0;
    counter = 0;
    lateness = 0;
    CoreTiming::AddTicks(CoreTiming::GetDowncount());
    CoreTiming::Advance();
    REQUIRE(MAX_SLICE_LENGTH == CoreTiming::GetDowncount());
    REQUIRE(0x1FULL == callbacks_ran_flags.to_ullong());
}

TEST_CASE("CoreTiming[PredictableLateness]", "[core]") {
    ScopeInit guard;

    CoreTiming::EventType* cb_a = CoreTiming::RegisterEvent("callbackA", CallbackTemplate<0>);
    CoreTiming::EventType* cb_b = CoreTiming::RegisterEvent("callbackB", CallbackTemplate<1>);

    // Enter slice 0
    CoreTiming::Advance();

    CoreTiming::ScheduleEvent(100, cb_a, CB_IDS[0]);
    CoreTiming::ScheduleEvent(200, cb_b, CB_IDS[1]);

    AdvanceAndCheck(0, 90, 10, -10); // (100 - 10)
    AdvanceAndCheck(1, MAX_SLICE_LENGTH, 50, -50);
}

namespace ChainSchedulingTest {
static int reschedules = 0;

static void RescheduleCallback(u64 userdata, s64 cycles_late) {
    --reschedules;
    REQUIRE(reschedules >= 0);
    REQUIRE(lateness == cycles_late);

    if (reschedules > 0)
        CoreTiming::ScheduleEvent(1000, reinterpret_cast<CoreTiming::EventType*>(userdata),
                                  userdata);
}
} // namespace ChainSchedulingTest

TEST_CASE("CoreTiming[ChainScheduling]", "[core]") {
    using namespace ChainSchedulingTest;

    ScopeInit guard;

    CoreTiming::EventType* cb_a = CoreTiming::RegisterEvent("callbackA", CallbackTemplate<0>);
    CoreTiming::EventType* cb_b = CoreTiming::RegisterEvent("callbackB", CallbackTemplate<1>);
    CoreTiming::EventType* cb_c = CoreTiming::RegisterEvent("callbackC", CallbackTemplate<2>);
    CoreTiming::EventType* cb_rs =
        CoreTiming::RegisterEvent("callbackReschedule", RescheduleCallback);

    // Enter slice 0
    CoreTiming::Advance();

    CoreTiming::ScheduleEvent(800, cb_a, CB_IDS[0]);
    CoreTiming::ScheduleEvent(1000, cb_b, CB_IDS[1]);
    CoreTiming::ScheduleEvent(2200, cb_c, CB_IDS[2]);
    CoreTiming::ScheduleEvent(1000, cb_rs, reinterpret_cast<u64>(cb_rs));
    REQUIRE(800 == CoreTiming::GetDowncount());

    reschedules = 3;
    AdvanceAndCheck(0, 200);  // cb_a
    AdvanceAndCheck(1, 1000); // cb_b, cb_rs
    REQUIRE(2 == reschedules);

    CoreTiming::AddTicks(CoreTiming::GetDowncount());
    CoreTiming::Advance(); // cb_rs
    REQUIRE(1 == reschedules);
    REQUIRE(200 == CoreTiming::GetDowncount());

    AdvanceAndCheck(2, 800); // cb_c

    CoreTiming::AddTicks(CoreTiming::GetDowncount());
    CoreTiming::Advance(); // cb_rs
    REQUIRE(0 == reschedules);
    REQUIRE(MAX_SLICE_LENGTH == CoreTiming::GetDowncount());
}
