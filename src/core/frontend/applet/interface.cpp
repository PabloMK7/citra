// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <unordered_map>
#include "core/frontend/interface.h"

namespace Frontend {

std::unordered_map<AppletType, std::shared_ptr<AppletInterface>> registered_applets;

void RegisterFrontendApplet(std::shared_ptr<AppletInterface> applet, AppletType type) {
    registered_applets[type] = applet;
}

void UnregisterFrontendApplet(AppletType type) {
    registered_applets.erase(type);
}

} // namespace Frontend
