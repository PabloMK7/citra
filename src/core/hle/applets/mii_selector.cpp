// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <string>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/hle/applets/mii_selector.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/result.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace HLE::Applets {

ResultCode MiiSelector::ReceiveParameter(const Service::APT::MessageParameter& parameter) {
    if (parameter.signal != Service::APT::SignalType::Request) {
        LOG_ERROR(Service_APT, "unsupported signal {}", static_cast<u32>(parameter.signal));
        UNIMPLEMENTED();
        // TODO(Subv): Find the right error code
        return ResultCode(-1);
    }

    // The LibAppJustStarted message contains a buffer with the size of the framebuffer shared
    // memory.
    // Create the SharedMemory that will hold the framebuffer data
    Service::APT::CaptureBufferInfo capture_info;
    ASSERT(sizeof(capture_info) == parameter.buffer.size());

    memcpy(&capture_info, parameter.buffer.data(), sizeof(capture_info));

    using Kernel::MemoryPermission;
    // Create a SharedMemory that directly points to this heap block.
    framebuffer_memory = Core::System::GetInstance().Kernel().CreateSharedMemoryForApplet(
        0, capture_info.size, MemoryPermission::ReadWrite, MemoryPermission::ReadWrite,
        "MiiSelector Memory");

    // Send the response message with the newly created SharedMemory
    Service::APT::MessageParameter result;
    result.signal = Service::APT::SignalType::Response;
    result.buffer.clear();
    result.destination_id = Service::APT::AppletId::Application;
    result.sender_id = id;
    result.object = framebuffer_memory;

    SendParameter(result);
    return RESULT_SUCCESS;
}

ResultCode MiiSelector::StartImpl(const Service::APT::AppletStartupParameter& parameter) {
    is_running = true;

    // TODO(Subv): Reverse the parameter format for the Mii Selector

    memcpy(&config, parameter.buffer.data(), parameter.buffer.size());

    // This data was obtained by writing the returned buffer in AppletManager::GlanceParameter of
    // the LLEd Mii picker of version system version 11.8.0 to a file and then matching the values
    // to the members of the MiiResult struct
    MiiData mii_data;
    mii_data.mii_id = 0x03001030;
    mii_data.system_id = 0xD285B6B300C8850A;
    mii_data.specialness_and_creation_date = 0x98391EE4;
    mii_data.creator_mac = {0x40, 0xF4, 0x07, 0xB7, 0x37, 0x10};
    mii_data.padding = 0x0;
    mii_data.mii_information = 0xA600;
    mii_data.mii_name = {'C', 'i', 't', 'r', 'a', 0x0, 0x0, 0x0, 0x0, 0x0};
    mii_data.width_height = 0x4040;
    mii_data.appearance_bits1.raw = 0x0;
    mii_data.appearance_bits2.raw = 0x0;
    mii_data.hair_style = 0x21;
    mii_data.appearance_bits3.hair_color.Assign(0x1);
    mii_data.appearance_bits3.flip_hair.Assign(0x0);
    mii_data.unknown1 = 0x02684418;
    mii_data.appearance_bits4.eyebrow_style.Assign(0x6);
    mii_data.appearance_bits4.eyebrow_color.Assign(0x1);
    mii_data.appearance_bits5.eyebrow_scale.Assign(0x4);
    mii_data.appearance_bits5.eyebrow_yscale.Assign(0x3);
    mii_data.appearance_bits6 = 0x4614;
    mii_data.unknown2 = 0x81121768;
    mii_data.allow_copying = 0x0D;
    mii_data.unknown3 = {0x0, 0x0, 0x29, 0x0, 0x52, 0x48, 0x50};
    mii_data.author_name = {'f', 'l', 'T', 'o', 'b', 'i', 0x0, 0x0, 0x0, 0x0};

    MiiResult result;
    result.return_code = 0x0;
    result.is_guest_mii_selected = 0x0;
    result.selected_guest_mii_index = 0xFFFFFFFF;
    result.selected_mii_data = mii_data;
    result.unknown1 = 0x0;
    result.mii_data_checksum = 0x056C;
    result.guest_mii_name.fill(0x0);

    // Let the application know that we're closing
    Service::APT::MessageParameter message;
    message.buffer.resize(sizeof(MiiResult));
    std::memcpy(message.buffer.data(), &result, message.buffer.size());
    message.signal = Service::APT::SignalType::WakeupByExit;
    message.destination_id = Service::APT::AppletId::Application;
    message.sender_id = id;
    SendParameter(message);

    is_running = false;
    return RESULT_SUCCESS;
}

void MiiSelector::Update() {}
} // namespace HLE::Applets
