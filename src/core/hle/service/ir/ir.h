// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Core {
class System;
}

namespace SM {
class ServiceManager;
}

namespace Service::IR {

void InstallInterfaces(Core::System& system);

} // namespace Service::IR
