// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cctype>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/frontend/applets/swkbd.h"
#include "core/hle/service/cfg/cfg.h"

namespace Frontend {

ValidationError SoftwareKeyboard::ValidateFilters(const std::string& input) const {
    if (config.filters.prevent_digit) {
        if (std::any_of(input.begin(), input.end(),
                        [](unsigned char c) { return std::isdigit(c); })) {
            return ValidationError::DigitNotAllowed;
        }
    }
    if (config.filters.prevent_at) {
        if (input.find('@') != std::string::npos) {
            return ValidationError::AtSignNotAllowed;
        }
    }
    if (config.filters.prevent_percent) {
        if (input.find('%') != std::string::npos) {
            return ValidationError::PercentNotAllowed;
        }
    }
    if (config.filters.prevent_backslash) {
        if (input.find('\\') != std::string::npos) {
            return ValidationError::BackslashNotAllowed;
        }
    }
    if (config.filters.prevent_profanity) {
        // TODO: check the profanity filter
        LOG_INFO(Frontend, "App requested swkbd profanity filter, but its not implemented.");
    }
    if (config.filters.enable_callback) {
        // TODO: check the callback
        LOG_INFO(Frontend, "App requested a swkbd callback, but its not implemented.");
    }
    return ValidationError::None;
}

ValidationError SoftwareKeyboard::ValidateInput(const std::string& input) const {
    ValidationError error;
    if ((error = ValidateFilters(input)) != ValidationError::None) {
        return error;
    }

    // TODO(jroweboy): Is max_text_length inclusive or exclusive?
    if (input.size() > config.max_text_length) {
        return ValidationError::MaxLengthExceeded;
    }

    bool is_blank =
        std::all_of(input.begin(), input.end(), [](unsigned char c) { return std::isspace(c); });
    bool is_empty = input.empty();
    switch (config.accept_mode) {
    case AcceptedInput::FixedLength:
        if (input.size() != config.max_text_length) {
            return ValidationError::FixedLengthRequired;
        }
        break;
    case AcceptedInput::NotEmptyAndNotBlank:
        if (is_blank) {
            return ValidationError::BlankInputNotAllowed;
        }
        if (is_empty) {
            return ValidationError::EmptyInputNotAllowed;
        }
        break;
    case AcceptedInput::NotBlank:
        if (is_blank) {
            return ValidationError::BlankInputNotAllowed;
        }
        break;
    case AcceptedInput::NotEmpty:
        if (is_empty) {
            return ValidationError::EmptyInputNotAllowed;
        }
        break;
    case AcceptedInput::Anything:
        return ValidationError::None;
    default:
        // TODO(jroweboy): What does hardware do in this case?
        LOG_CRITICAL(Frontend, "Application requested unknown validation method. Method: {}",
                     static_cast<u32>(config.accept_mode));
        UNREACHABLE();
    }

    return ValidationError::None;
}

ValidationError SoftwareKeyboard::ValidateButton(u8 button) const {
    switch (config.button_config) {
    case ButtonConfig::None:
        return ValidationError::None;
    case ButtonConfig::Single:
        if (button != 0) {
            return ValidationError::ButtonOutOfRange;
        }
        break;
    case ButtonConfig::Dual:
        if (button > 1) {
            return ValidationError::ButtonOutOfRange;
        }
        break;
    case ButtonConfig::Triple:
        if (button > 2) {
            return ValidationError::ButtonOutOfRange;
        }
        break;
    default:
        UNREACHABLE();
    }
    return ValidationError::None;
}

ValidationError SoftwareKeyboard::Finalize(const std::string& text, u8 button) {
    ValidationError error;
    if ((error = ValidateInput(text)) != ValidationError::None) {
        return error;
    }
    if ((error = ValidateButton(button)) != ValidationError::None) {
        return error;
    }
    data = {text, button};
    return ValidationError::None;
}

void DefaultKeyboard::Setup(const Frontend::KeyboardConfig* config) {
    SoftwareKeyboard::Setup(config);

    auto cfg = Service::CFG::GetModule(Core::System::GetInstance());
    ASSERT_MSG(cfg, "CFG Module missing!");
    std::string username = Common::UTF16ToUTF8(cfg->GetUsername());
    switch (this->config.button_config) {
    case ButtonConfig::None:
    case ButtonConfig::Single:
        Finalize(username, 0);
        break;
    case ButtonConfig::Dual:
        Finalize(username, 1);
        break;
    case ButtonConfig::Triple:
        Finalize(username, 2);
        break;
    default:
        UNREACHABLE();
    }
}

void RegisterSoftwareKeyboard(std::shared_ptr<SoftwareKeyboard> applet) {
    Core::System::GetInstance().RegisterSoftwareKeyboard(applet);
}

std::shared_ptr<SoftwareKeyboard> GetRegisteredSoftwareKeyboard() {
    return Core::System::GetInstance().GetSoftwareKeyboard();
}

} // namespace Frontend
