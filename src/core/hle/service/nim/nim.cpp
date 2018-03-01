// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/nim/nim.h"
#include "core/hle/service/nim/nim_aoc.h"
#include "core/hle/service/nim/nim_s.h"
#include "core/hle/service/nim/nim_u.h"

namespace Service {
namespace NIM {

void InstallInterfaces(SM::ServiceManager& service_manager) {
    std::make_shared<NIM_AOC>()->InstallAsService(service_manager);
    std::make_shared<NIM_S>()->InstallAsService(service_manager);
    std::make_shared<NIM_U>()->InstallAsService(service_manager);
}

} // namespace NIM

} // namespace Service
