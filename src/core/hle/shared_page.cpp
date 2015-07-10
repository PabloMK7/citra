// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>

#include "core/hle/shared_page.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace SharedPage {

SharedPageDef shared_page;

void Init() {
    std::memset(&shared_page, 0, sizeof(shared_page));

    shared_page.running_hw = 0x1; // product
}

void Shutdown() {
}

} // namespace
