// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "core/frontend/applet/swkbd.h"
#include "core/hle/applets/applet.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/result.h"
#include "core/hle/service/apt/apt.h"

namespace HLE {
namespace Applets {

/// Maximum number of buttons that can be in the keyboard.
constexpr int MAX_BUTTON = 3;
/// Maximum button text length, in UTF-16 code units.
constexpr int MAX_BUTTON_TEXT_LEN = 16;
/// Maximum hint text length, in UTF-16 code units.
constexpr int MAX_HINT_TEXT_LEN = 64;
/// Maximum filter callback error message length, in UTF-16 code units.
constexpr int MAX_CALLBACK_MSG_LEN = 256;

/// Keyboard types
enum class SoftwareKeyboardType : u32 {
    NORMAL = 0, ///< Normal keyboard with several pages (QWERTY/accents/symbol/mobile)
    QWERTY,     ///< QWERTY keyboard only.
    NUMPAD,     ///< Number pad.
    WESTERN,    ///< On JPN systems, a text keyboard without Japanese input capabilities,
                /// otherwise same as SWKBD_TYPE_NORMAL.
};

/// Keyboard dialog buttons.
enum class SoftwareKeyboardButtonConfig : u32 {
    SINGLE_BUTTON = 0, ///< Ok button
    DUAL_BUTTON,       ///< Cancel | Ok buttons
    TRIPLE_BUTTON,     ///< Cancel | I Forgot | Ok buttons
    NO_BUTTON,         ///< No button (returned by swkbdInputText in special cases)
};

/// Accepted input types.
enum class SoftwareKeyboardValidInput : u32 {
    ANYTHING = 0,      ///< All inputs are accepted.
    NOTEMPTY,          ///< Empty inputs are not accepted.
    NOTEMPTY_NOTBLANK, ///< Empty or blank inputs (consisting solely of whitespace) are not
                       /// accepted.
    NOTBLANK, ///< Blank inputs (consisting solely of whitespace) are not accepted, but empty
              /// inputs are.
    FIXEDLEN, ///< The input must have a fixed length (specified by maxTextLength in
              /// swkbdInit).
};

/// Keyboard password modes.
enum class SoftwareKeyboardPasswordMode : u32 {
    NONE = 0,   ///< Characters are not concealed.
    HIDE,       ///< Characters are concealed immediately.
    HIDE_DELAY, ///< Characters are concealed a second after they've been typed.
};

/// Keyboard input filtering flags. Allows the caller to specify what input is explicitly not
/// allowed
namespace SoftwareKeyboardFilter {
enum Filter {
    DIGITS = 1,         ///< Disallow the use of more than a certain number of digits (0 or more)
    AT = 1 << 1,        ///< Disallow the use of the @ sign.
    PERCENT = 1 << 2,   ///< Disallow the use of the % sign.
    BACKSLASH = 1 << 3, ///< Disallow the use of the \ sign.
    PROFANITY = 1 << 4, ///< Disallow profanity using Nintendo's profanity filter.
    CALLBACK = 1 << 5,  ///< Use a callback in order to check the input.
};
} // namespace SoftwareKeyboardFilter

/// Keyboard features.
namespace SoftwareKeyboardFeature {
enum Feature {
    PARENTAL = 1,               ///< Parental PIN mode.
    DARKEN_TOP_SCREEN = 1 << 1, ///< Darken the top screen when the keyboard is shown.
    PREDICTIVE_INPUT =
        1 << 2,           ///< Enable predictive input (necessary for Kanji input in JPN systems).
    MULTILINE = 1 << 3,   ///< Enable multiline input.
    FIXED_WIDTH = 1 << 4, ///< Enable fixed-width mode.
    ALLOW_HOME = 1 << 5,  ///< Allow the usage of the HOME button.
    ALLOW_RESET = 1 << 6, ///< Allow the usage of a software-reset combination.
    ALLOW_POWER = 1 << 7, ///< Allow the usage of the POWER button.
    DEFAULT_QWERTY = 1 << 9, ///< Default to the QWERTY page when the keyboard is shown.
};
} // namespace SoftwareKeyboardFeature

/// Keyboard filter callback return values.
enum class SoftwareKeyboardCallbackResult : u32 {
    OK = 0,   ///< Specifies that the input is valid.
    CLOSE,    ///< Displays an error message, then closes the keyboard.
    CONTINUE, ///< Displays an error message and continues displaying the keyboard.
};

/// Keyboard return values.
enum class SoftwareKeyboardResult : s32 {
    NONE = -1,          ///< Dummy/unused.
    INVALID_INPUT = -2, ///< Invalid parameters to swkbd.
    OUTOFMEM = -3,      ///< Out of memory.

    D0_CLICK = 0, ///< The button was clicked in 1-button dialogs.
    D1_CLICK0,    ///< The left button was clicked in 2-button dialogs.
    D1_CLICK1,    ///< The right button was clicked in 2-button dialogs.
    D2_CLICK0,    ///< The left button was clicked in 3-button dialogs.
    D2_CLICK1,    ///< The middle button was clicked in 3-button dialogs.
    D2_CLICK2,    ///< The right button was clicked in 3-button dialogs.

    HOMEPRESSED = 10, ///< The HOME button was pressed.
    RESETPRESSED,     ///< The soft-reset key combination was pressed.
    POWERPRESSED,     ///< The POWER button was pressed.

    PARENTAL_OK = 20, ///< The parental PIN was verified successfully.
    PARENTAL_FAIL,    ///< The parental PIN was incorrect.

    BANNED_INPUT = 30, ///< The filter callback returned SoftwareKeyboardCallback::CLOSE.
};

struct SoftwareKeyboardConfig {
    SoftwareKeyboardType type;
    SoftwareKeyboardButtonConfig num_buttons_m1;
    SoftwareKeyboardValidInput valid_input;
    SoftwareKeyboardPasswordMode password_mode;
    s32 is_parental_screen;
    s32 darken_top_screen;
    u32 filter_flags;
    u32 save_state_flags;
    u16 max_text_length;
    u16 dict_word_count;
    u16 max_digits;
    std::array<std::array<u16, MAX_BUTTON_TEXT_LEN + 1>, MAX_BUTTON> button_text;
    std::array<u16, 2> numpad_keys;
    std::array<u16, MAX_HINT_TEXT_LEN + 1>
        hint_text; ///< Text to display when asking the user for input
    bool predictive_input;
    bool multiline;
    bool fixed_width;
    bool allow_home;
    bool allow_reset;
    bool allow_power;
    bool unknown;
    bool default_qwerty;
    std::array<bool, 4> button_submits_text;
    u16 language;

    u32 initial_text_offset; ///< Offset of the default text in the output SharedMemory
    u32 dict_offset;
    u32 initial_status_offset;
    u32 initial_learning_offset;
    u32 shared_memory_size; ///< Size of the SharedMemory
    u32 version;

    SoftwareKeyboardResult return_code;

    u32 status_offset;
    u32 learning_offset;

    u32 text_offset; ///< Offset in the SharedMemory where the output text starts
    u16 text_length; ///< Length in characters of the output text

    int callback_result;
    std::array<u16, MAX_CALLBACK_MSG_LEN + 1> callback_msg;
    bool skip_at_check;
    INSERT_PADDING_BYTES(0xAB);
};

/**
 * The size of this structure (0x400) has been verified via reverse engineering of multiple games
 * that use the software keyboard.
 */
static_assert(sizeof(SoftwareKeyboardConfig) == 0x400, "Software Keyboard Config size is wrong");

class DefaultCitraKeyboard : Frontend::AppletInterface {};

class SoftwareKeyboard final : public Applet {
public:
    SoftwareKeyboard(Service::APT::AppletId id, std::weak_ptr<Service::APT::AppletManager> manager)
        : Applet(id, std::move(manager)) {}

    ResultCode ReceiveParameter(const Service::APT::MessageParameter& parameter) override;
    ResultCode StartImpl(const Service::APT::AppletStartupParameter& parameter) override;
    void Update() override;

    /**
     * Draws a keyboard to the current bottom screen framebuffer.
     */
    void DrawScreenKeyboard();

    /**
     * Sends the LibAppletClosing signal to the application,
     * along with the relevant data buffers.
     */
    void Finalize();

private:
    /// This SharedMemory will be created when we receive the LibAppJustStarted message.
    /// It holds the framebuffer info retrieved by the application with
    /// GSPGPU::ImportDisplayCaptureInfo
    Kernel::SharedPtr<Kernel::SharedMemory> framebuffer_memory;

    /// SharedMemory where the output text will be stored
    Kernel::SharedPtr<Kernel::SharedMemory> text_memory;

    /// Configuration of this instance of the SoftwareKeyboard, as received from the application
    SoftwareKeyboardConfig config;
};
} // namespace Applets
} // namespace HLE
