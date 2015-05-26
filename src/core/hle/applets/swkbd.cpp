// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"

#include "core/hle/applets/swkbd.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace HLE {
namespace Applets {

SoftwareKeyboard::SoftwareKeyboard(Service::APT::AppletId id) : Applet(id) {
    // Create the SharedMemory that will hold the framebuffer data
    // TODO(Subv): What size should we use here?
    using Kernel::MemoryPermission;
    framebuffer_memory = Kernel::SharedMemory::Create(0x1000, MemoryPermission::ReadWrite, MemoryPermission::ReadWrite, "SoftwareKeyboard Memory");
}

ResultCode SoftwareKeyboard::ReceiveParameter(Service::APT::MessageParameter const& parameter) {
    if (parameter.signal != static_cast<u32>(Service::APT::SignalType::LibAppJustStarted)) {
        LOG_ERROR(Service_APT, "unsupported signal %u", parameter.signal);
        UNIMPLEMENTED();
        // TODO(Subv): Find the right error code
        return ResultCode(-1);
    }

    Service::APT::MessageParameter result;
    // The buffer passed in parameter contains the data returned by GSPGPU::ImportDisplayCaptureInfo
    result.signal = static_cast<u32>(Service::APT::SignalType::LibAppFinished);
    result.data = nullptr; 
    result.buffer_size = 0;
    result.destination_id = static_cast<u32>(Service::APT::AppletId::Application);
    result.sender_id = static_cast<u32>(id);
    result.object = framebuffer_memory;

    Service::APT::SendParameter(result);
    return RESULT_SUCCESS;
}

ResultCode SoftwareKeyboard::Start(Service::APT::AppletStartupParameter const& parameter) {
    memcpy(&config, parameter.data, parameter.buffer_size);
    text_memory = boost::static_pointer_cast<Kernel::SharedMemory, Kernel::Object>(parameter.object);

    // TODO(Subv): Verify if this is the correct behavior
    memset(text_memory->GetPointer(), 0, text_memory->size);

    // TODO(Subv): Remove this hardcoded text
    const wchar_t str[] = L"Subv";
    memcpy(text_memory->GetPointer(), str, 4 * sizeof(wchar_t));
    
    // TODO(Subv): Ask for input and write it to the shared memory
    // TODO(Subv): Find out what are the possible values for the return code,
    // some games seem to check for a hardcoded 2
    config.return_code = 2;
    config.text_length = 5;
    config.text_offset = 0;

    Service::APT::MessageParameter message;
    message.buffer_size = sizeof(SoftwareKeyboardConfig);
    message.data = reinterpret_cast<u8*>(&config);
    message.signal = static_cast<u32>(Service::APT::SignalType::LibAppClosed);
    message.destination_id = static_cast<u32>(Service::APT::AppletId::Application);
    message.sender_id = static_cast<u32>(id);
    Service::APT::SendParameter(message);

    return RESULT_SUCCESS;
}

}
} // namespace
