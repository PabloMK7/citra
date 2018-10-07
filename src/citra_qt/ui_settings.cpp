// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "ui_settings.h"

namespace UISettings {

const Themes themes{{
    {"Default", "default"},
    {"Dark", "qdarkstyle"},
    {"Colorful", "colorful"},
    {"Colorful Dark", "colorful_dark"},
}};

Values values = {};

} // namespace UISettings
