// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/frontend/applet/swkbd.h"

namespace Frontend {

ValidationError SoftwareKeyboard::ValidateFilters(const std::string& input) {
    if (config.filters.prevent_digit) {
        if (std::any_of(input.begin(), input.end(), std::isdigit)) {
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
    if (config.filter.prevent_backslash) {
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
    return valid;
}

ValidationError SoftwareKeyboard::ValidateInput(const std::string& input) {
    ValidationError error;
    if ((error = ValidateFilters(input)) != ValidationError::None) {
        return error;
    }

    // TODO(jroweboy): Is max_text_length inclusive or exclusive?
    if (input.size() > config.max_text_length) {
        return ValidationError::MaxLengthExceeded;
    }

    auto is_blank = [&] { return std::all_of(input.begin(), input.end(), std::isspace); };
    auto is_empty = [&] { return input.empty(); };
    switch (config.valid_input) {
    case AcceptedInput::FixedLength:
        if (input.size() != config.max_text_length) {
            return ValidationError::FixedLengthRequired;
        }
        break;
    case AcceptedInput::NotEmptyAndNotBlank:
        if (is_blank()) {
            return ValidationError::BlankInputNotAllowed;
        }
        if (is_empty()) {
            return ValidationError::EmptyInputNotAllowed;
        }
        break;
    case AcceptedInput::NotBlank:
        if (is_blank()) {
            return ValidationError::BlankInputNotAllowed;
        }
        break;
    case AcceptedInput::NotEmpty:
        if (is_empty()) {
            return ValidationError::EmptyInputNotAllowed;
        }
        break;
    case AcceptedInput::Anything:
        return ValidationError::None;
    default:
        // TODO(jroweboy): What does hardware do in this case?
        NGLOG_CRITICAL(Frontend, "Application requested unknown validation method. Method: {}",
                       static_cast<u32>(config.valid_input));
        UNREACHABLE();
    }

    return ValidationError::None;
} // namespace Frontend

ValidationError SoftwareKeyboard::ValidateButton(u8 button) {
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

ValidationError Finalize(cosnt std::string& text, u8 button) {
    ValidationError error;
    if ((error = ValidateInput(text)) != ValidationError::NONE) {
        return error;
    }
    if ((error = ValidateButton(button)) != ValidationError::NONE) {
        return error;
    }
    data = {text, button};
    running = false;
}

} // namespace Frontend
