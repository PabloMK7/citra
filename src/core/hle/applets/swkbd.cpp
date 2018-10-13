// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include <string>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/hle/applets/swkbd.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/result.h"
#include "core/hle/service/gsp/gsp.h"
#include "core/hle/service/hid/hid.h"
#include "core/memory.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace HLE {
namespace Applets {

ResultCode SoftwareKeyboard::ReceiveParameter(Service::APT::MessageParameter const& parameter) {
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
    // Allocate a heap block of the required size for this applet.
    heap_memory = std::make_shared<std::vector<u8>>(capture_info.size);
    // Create a SharedMemory that directly points to this heap block.
    framebuffer_memory = Core::System::GetInstance().Kernel().CreateSharedMemoryForApplet(
        heap_memory, 0, capture_info.size, MemoryPermission::ReadWrite, MemoryPermission::ReadWrite,
        "SoftwareKeyboard Memory");

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

ResultCode SoftwareKeyboard::StartImpl(Service::APT::AppletStartupParameter const& parameter) {
    ASSERT_MSG(parameter.buffer.size() == sizeof(config),
               "The size of the parameter (SoftwareKeyboardConfig) is wrong");

    memcpy(&config, parameter.buffer.data(), parameter.buffer.size());
    text_memory =
        boost::static_pointer_cast<Kernel::SharedMemory, Kernel::Object>(parameter.object);

    // TODO(Subv): Verify if this is the correct behavior
    memset(text_memory->GetPointer(), 0, text_memory->size);

    DrawScreenKeyboard();

    using namespace Frontend;
    frontend_applet = GetRegisteredSoftwareKeyboard();
    if (frontend_applet) {
        KeyboardConfig frontend_config = ToFrontendConfig(config);
        frontend_applet->Setup(&frontend_config);
    }

    is_running = true;
    return RESULT_SUCCESS;
}

void SoftwareKeyboard::Update() {
    using namespace Frontend;
    KeyboardData data(*frontend_applet->ReceiveData());
    std::u16string text = Common::UTF8ToUTF16(data.text);
    memcpy(text_memory->GetPointer(), text.c_str(), text.length() * sizeof(char16_t));
    switch (config.num_buttons_m1) {
    case SoftwareKeyboardButtonConfig::SingleButton:
        config.return_code = SoftwareKeyboardResult::D0Click;
        break;
    case SoftwareKeyboardButtonConfig::DualButton:
        if (data.button == 0)
            config.return_code = SoftwareKeyboardResult::D1Click0;
        else
            config.return_code = SoftwareKeyboardResult::D1Click1;
        break;
    case SoftwareKeyboardButtonConfig::TripleButton:
        if (data.button == 0)
            config.return_code = SoftwareKeyboardResult::D2Click0;
        else if (data.button == 1)
            config.return_code = SoftwareKeyboardResult::D2Click1;
        else
            config.return_code = SoftwareKeyboardResult::D2Click2;
        break;
    case SoftwareKeyboardButtonConfig::NoButton:
        // TODO: find out what is actually returned
        config.return_code = SoftwareKeyboardResult::None;
        break;
    default:
        LOG_CRITICAL(Applet_SWKBD, "Unknown button config {}",
                     static_cast<u32>(config.num_buttons_m1));
        UNREACHABLE();
    }

    config.text_length = static_cast<u16>(text.size());
    config.text_offset = 0;

    // TODO(Subv): We're finalizing the applet immediately after it's started,
    // but we should defer this call until after all the input has been collected.
    Finalize();
}

void SoftwareKeyboard::DrawScreenKeyboard() {
    // TODO(Subv): Draw the HLE keyboard, for now just do nothing
}

void SoftwareKeyboard::Finalize() {
    // Let the application know that we're closing
    Service::APT::MessageParameter message;
    message.buffer.resize(sizeof(SoftwareKeyboardConfig));
    std::memcpy(message.buffer.data(), &config, message.buffer.size());
    message.signal = Service::APT::SignalType::WakeupByExit;
    message.destination_id = Service::APT::AppletId::Application;
    message.sender_id = id;
    SendParameter(message);

    is_running = false;
}

Frontend::KeyboardConfig SoftwareKeyboard::ToFrontendConfig(
    const SoftwareKeyboardConfig& config) const {
    using namespace Frontend;
    KeyboardConfig frontend_config;
    frontend_config.button_config =
        static_cast<ButtonConfig>(static_cast<u32>(config.num_buttons_m1));
    frontend_config.accept_mode = static_cast<AcceptedInput>(static_cast<u32>(config.valid_input));
    frontend_config.multiline_mode = config.multiline;
    frontend_config.max_text_length = config.max_text_length;
    frontend_config.max_digits = config.max_digits;

    std::size_t text_size = config.hint_text.size();
    const auto text_end = std::find(config.hint_text.begin(), config.hint_text.end(), u'\0');
    if (text_end != config.hint_text.end())
        text_size = std::distance(config.hint_text.begin(), text_end);
    std::u16string buffer(text_size, 0);
    std::memcpy(buffer.data(), config.hint_text.data(), text_size * sizeof(u16));
    frontend_config.hint_text = Common::UTF16ToUTF8(buffer);
    frontend_config.has_custom_button_text =
        !std::all_of(config.button_text.begin(), config.button_text.end(),
                     [](std::array<u16, HLE::Applets::MAX_BUTTON_TEXT_LEN + 1> x) {
                         return std::all_of(x.begin(), x.end(), [](u16 x) { return x == 0; });
                     });
    if (frontend_config.has_custom_button_text) {
        for (const auto& text : config.button_text) {
            text_size = text.size();
            const auto text_end = std::find(text.begin(), text.end(), u'\0');
            if (text_end != text.end())
                text_size = std::distance(text.begin(), text_end);

            buffer.resize(text_size);
            std::memcpy(buffer.data(), text.data(), text_size * sizeof(u16));
            frontend_config.button_text.push_back(Common::UTF16ToUTF8(buffer));
        }
    }
    frontend_config.filters.prevent_digit =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::Digits);
    frontend_config.filters.prevent_at =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::At);
    frontend_config.filters.prevent_percent =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::Percent);
    frontend_config.filters.prevent_backslash =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::Backslash);
    frontend_config.filters.prevent_profanity =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::Profanity);
    frontend_config.filters.enable_callback =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::Callback);
    return frontend_config;
}
} // namespace Applets
} // namespace HLE
