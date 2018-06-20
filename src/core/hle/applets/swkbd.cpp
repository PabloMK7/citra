// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <string>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/string_util.h"
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
    framebuffer_memory = Kernel::SharedMemory::CreateForApplet(
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
    frontend_applet = GetRegisteredApplet(AppletType::SoftwareKeyboard);
    if (frontend_applet) {
        KeyboardConfig frontend_config = ToFrontendConfig(config);
        frontend_applet->Setup(&frontend_config);
    }

    is_running = true;
    return RESULT_SUCCESS;
}

void SoftwareKeyboard::Update() {
    using namespace Frontend;
    KeyboardData data(*static_cast<const KeyboardData*>(frontend_applet->ReceiveData()));
    std::u16string text = Common::UTF8ToUTF16(data.text);
    memcpy(text_memory->GetPointer(), text.c_str(), text.length() * sizeof(char16_t));
    switch (config.num_buttons_m1) {
    case SoftwareKeyboardButtonConfig::SINGLE_BUTTON:
        config.return_code = SoftwareKeyboardResult::D0_CLICK;
        break;
    case SoftwareKeyboardButtonConfig::DUAL_BUTTON:
        if (data.button == 0)
            config.return_code = SoftwareKeyboardResult::D1_CLICK0;
        else
            config.return_code = SoftwareKeyboardResult::D1_CLICK1;
        break;
    case SoftwareKeyboardButtonConfig::TRIPLE_BUTTON:
        if (data.button == 0)
            config.return_code = SoftwareKeyboardResult::D2_CLICK0;
        else if (data.button == 1)
            config.return_code = SoftwareKeyboardResult::D2_CLICK1;
        else
            config.return_code = SoftwareKeyboardResult::D2_CLICK2;
        break;
    case SoftwareKeyboardButtonConfig::NO_BUTTON:
        // TODO: find out what is actually returned
        config.return_code = SoftwareKeyboardResult::NONE;
        break;
    default:
        NGLOG_CRITICAL(Applet_SWKBD, "Unknown button config {}",
                       static_cast<int>(config.num_buttons_m1));
        UNREACHABLE();
    }

    config.text_length = static_cast<u16>(text.size() + 1);
    config.text_offset = 0;

    // TODO(Subv): We're finalizing the applet immediately after it's started,
    // but we should defer this call until after all the input has been collected.
    Finalize();
}

void SoftwareKeyboard::DrawScreenKeyboard() {
    auto bottom_screen = Service::GSP::GetFrameBufferInfo(0, 1);
    auto info = bottom_screen->framebuffer_info[bottom_screen->index];

    // TODO(Subv): Draw the HLE keyboard, for now just zero-fill the framebuffer
    Memory::ZeroBlock(info.address_left, info.stride * 320);

    Service::GSP::SetBufferSwap(1, info);
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

Frontend::KeyboardConfig SoftwareKeyboard::ToFrontendConfig(SoftwareKeyboardConfig config) {
    using namespace Frontend;
    KeyboardConfig frontend_config;
    frontend_config.button_config = static_cast<ButtonConfig>(config.num_buttons_m1);
    frontend_config.accept_mode = static_cast<AcceptedInput>(config.valid_input);
    frontend_config.multiline_mode = config.multiline;
    frontend_config.max_text_length = config.max_text_length;
    frontend_config.max_digits = config.max_digits;
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
    frontend_config.hint_text =
        convert.to_bytes(reinterpret_cast<const char16_t*>(config.hint_text.data()));
    frontend_config.has_custom_button_text =
        std::all_of(config.button_text.begin(), config.button_text.end(),
                    [](std::array<u16, HLE::Applets::MAX_BUTTON_TEXT_LEN + 1> x) {
                        return std::all_of(x.begin(), x.end(), [](u16 x) { return x == 0; });
                    });
    if (frontend_config.has_custom_button_text) {
        for (const auto& text : config.button_text) {
            frontend_config.button_text.push_back(
                convert.to_bytes(reinterpret_cast<const char16_t*>(text.data())));
        }
    }
    frontend_config.filters.prevent_digit =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::DIGITS);
    frontend_config.filters.prevent_at =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::AT);
    frontend_config.filters.prevent_percent =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::PERCENT);
    frontend_config.filters.prevent_backslash =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::BACKSLASH);
    frontend_config.filters.prevent_profanity =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::PROFANITY);
    frontend_config.filters.enable_callback =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::CALLBACK);
    return frontend_config;
}
} // namespace Applets
} // namespace HLE
