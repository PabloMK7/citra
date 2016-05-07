// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <string>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/string_util.h"

#include "core/hle/applets/mii_selector.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/result.h"

#include "video_core/video_core.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace HLE {
namespace Applets {

ResultCode MiiSelector::ReceiveParameter(const Service::APT::MessageParameter& parameter) {
    if (parameter.signal != static_cast<u32>(Service::APT::SignalType::LibAppJustStarted)) {
        LOG_ERROR(Service_APT, "unsupported signal %u", parameter.signal);
        UNIMPLEMENTED();
        // TODO(Subv): Find the right error code
        return ResultCode(-1);
    }

    // The LibAppJustStarted message contains a buffer with the size of the framebuffer shared memory.
    // Create the SharedMemory that will hold the framebuffer data
    Service::APT::CaptureBufferInfo capture_info;
    ASSERT(sizeof(capture_info) == parameter.buffer_size);

    memcpy(&capture_info, parameter.data, sizeof(capture_info));
    using Kernel::MemoryPermission;
    framebuffer_memory = Kernel::SharedMemory::Create(capture_info.size, MemoryPermission::ReadWrite,
                                                      MemoryPermission::ReadWrite, "MiiSelector Memory");

    // Send the response message with the newly created SharedMemory
    Service::APT::MessageParameter result;
    result.signal = static_cast<u32>(Service::APT::SignalType::LibAppFinished);
    result.data = nullptr;
    result.buffer_size = 0;
    result.destination_id = static_cast<u32>(Service::APT::AppletId::Application);
    result.sender_id = static_cast<u32>(id);
    result.object = framebuffer_memory;

    Service::APT::SendParameter(result);
    return RESULT_SUCCESS;
}

ResultCode MiiSelector::StartImpl(const Service::APT::AppletStartupParameter& parameter) {
    started = true;

    // TODO(Subv): Set the expected fields in the response buffer before resending it to the application.
    // TODO(Subv): Reverse the parameter format for the Mii Selector

    if(parameter.buffer_size >= sizeof(u32)) {
        // TODO: defaults return no error, but garbage in other unknown fields
        memset(parameter.data, 0, sizeof(u32));
    }

    // Let the application know that we're closing
    Service::APT::MessageParameter message;
    message.buffer_size = parameter.buffer_size;
    message.data = parameter.data;
    message.signal = static_cast<u32>(Service::APT::SignalType::LibAppClosed);
    message.destination_id = static_cast<u32>(Service::APT::AppletId::Application);
    message.sender_id = static_cast<u32>(id);
    Service::APT::SendParameter(message);

    started = false;
    return RESULT_SUCCESS;
}

void MiiSelector::Update() {
}

}
} // namespace
