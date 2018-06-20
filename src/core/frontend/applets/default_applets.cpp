// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/frontend/applets/default_applets.h"
#include "core/frontend/applets/interface.h"
#include "core/frontend/applets/swkbd.h"

namespace Frontend {
void RegisterDefaultApplets() {
    RegisterFrontendApplet(std::make_shared<DefaultCitraKeyboard>(), AppletType::SoftwareKeyboard);
}
} // namespace Frontend
