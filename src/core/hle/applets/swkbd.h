// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "core/hle/applets/applet.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/result.h"
#include "core/hle/service/apt/apt.h"

namespace HLE {
namespace Applets {

struct SoftwareKeyboardConfig {
    INSERT_PADDING_WORDS(0x8);

    u16 max_text_length; ///< Maximum length of the input text

    INSERT_PADDING_BYTES(0x6E);

    char16_t display_text[65]; ///< Text to display when asking the user for input

    INSERT_PADDING_BYTES(0xE);

    u32 default_text_offset; ///< Offset of the default text in the output SharedMemory

    INSERT_PADDING_WORDS(0x3);

    u32 shared_memory_size; ///< Size of the SharedMemory

    INSERT_PADDING_WORDS(0x1);

    u32 return_code; ///< Return code of the SoftwareKeyboard, usually 2, other values are unknown

    INSERT_PADDING_WORDS(0x2);

    u32 text_offset; ///< Offset in the SharedMemory where the output text starts
    u16 text_length; ///< Length in characters of the output text

    INSERT_PADDING_BYTES(0x2B6);
};

/**
 * The size of this structure (0x400) has been verified via reverse engineering of multiple games
 * that use the software keyboard.
 */
static_assert(sizeof(SoftwareKeyboardConfig) == 0x400, "Software Keyboard Config size is wrong");

class SoftwareKeyboard final : public Applet {
public:
    SoftwareKeyboard(Service::APT::AppletId id) : Applet(id), started(false) {}

    ResultCode ReceiveParameter(const Service::APT::MessageParameter& parameter) override;
    ResultCode StartImpl(const Service::APT::AppletStartupParameter& parameter) override;
    void Update() override;
    bool IsRunning() const override {
        return started;
    }

    /**
     * Draws a keyboard to the current bottom screen framebuffer.
     */
    void DrawScreenKeyboard();

    /**
     * Sends the LibAppletClosing signal to the application,
     * along with the relevant data buffers.
     */
    void Finalize();

    /// This SharedMemory will be created when we receive the LibAppJustStarted message.
    /// It holds the framebuffer info retrieved by the application with
    /// GSPGPU::ImportDisplayCaptureInfo
    Kernel::SharedPtr<Kernel::SharedMemory> framebuffer_memory;

    /// SharedMemory where the output text will be stored
    Kernel::SharedPtr<Kernel::SharedMemory> text_memory;

    /// Configuration of this instance of the SoftwareKeyboard, as received from the application
    SoftwareKeyboardConfig config;

    /// Whether this applet is currently running instead of the host application or not.
    bool started;
};
}
} // namespace
