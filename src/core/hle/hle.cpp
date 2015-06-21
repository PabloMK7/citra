// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"

#include "core/arm/arm_interface.h"
#include "core/core.h"
#include "core/hle/hle.h"
#include "core/hle/config_mem.h"
#include "core/hle/shared_page.h"
#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace HLE {

bool g_reschedule; ///< If true, immediately reschedules the CPU to a new thread

void Reschedule(const char *reason) {
    DEBUG_ASSERT_MSG(reason != nullptr && strlen(reason) < 256, "Reschedule: Invalid or too long reason.");

    // TODO(bunnei): It seems that games depend on some CPU execution time elapsing during HLE
    // routines. This simulates that time by artificially advancing the number of CPU "ticks".
    // The value was chosen empirically, it seems to work well enough for everything tested, but
    // is likely not ideal. We should find a more accurate way to simulate timing with HLE.
    Core::g_app_core->AddTicks(4000);

    Core::g_app_core->PrepareReschedule();

    g_reschedule = true;
}

void Init() {
    Service::Init();
    ConfigMem::Init();
    SharedPage::Init();

    g_reschedule = false;

    LOG_DEBUG(Kernel, "initialized OK");
}

void Shutdown() {
    ConfigMem::Shutdown();
    SharedPage::Shutdown();
    Service::Shutdown();

    LOG_DEBUG(Kernel, "shutdown OK");
}

} // namespace
