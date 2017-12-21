// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <vector>
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/gsp/gsp.h"

namespace Service {
namespace GSP {

static std::weak_ptr<GSP_GPU> gsp_gpu;

FrameBufferUpdate* GetFrameBufferInfo(u32 thread_id, u32 screen_index) {
    auto gpu = gsp_gpu.lock();
    ASSERT(gpu != nullptr);
    return gpu->GetFrameBufferInfo(thread_id, screen_index);
}

void SignalInterrupt(InterruptId interrupt_id) {
    auto gpu = gsp_gpu.lock();
    ASSERT(gpu != nullptr);
    return gpu->SignalInterrupt(interrupt_id);
}

void InstallInterfaces(SM::ServiceManager& service_manager) {
    auto gpu = std::make_shared<GSP_GPU>();
    gpu->InstallAsService(service_manager);

    gsp_gpu = gpu;
}

} // namespace GSP
} // namespace Service
