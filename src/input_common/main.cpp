// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include "common/param_package.h"
#include "input_common/keyboard.h"
#include "input_common/main.h"

namespace InputCommon {

static std::shared_ptr<Keyboard> keyboard;

void Init() {
    keyboard = std::make_shared<InputCommon::Keyboard>();
    Input::RegisterFactory<Input::ButtonDevice>("keyboard", keyboard);
}

void Shutdown() {
    Input::UnregisterFactory<Input::ButtonDevice>("keyboard");
    keyboard.reset();
}

Keyboard* GetKeyboard() {
    return keyboard.get();
}

std::string GenerateKeyboardParam(int key_code) {
    Common::ParamPackage param{
        {"engine", "keyboard"}, {"code", std::to_string(key_code)},
    };
    return param.Serialize();
}

} // namespace InputCommon
