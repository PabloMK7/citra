// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace SM {
class ServiceManager;
}

namespace Service {
namespace IR {

/// Reload input devices. Used when input configuration changed
void ReloadInputDevices();

void InstallInterfaces(SM::ServiceManager& service_manager);

} // namespace IR
} // namespace Service
