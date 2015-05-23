// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string.h>

#include "pica.h"

namespace Pica {

State g_state;

void Init() {
}

void Shutdown() {
    memset(&g_state, 0, sizeof(State));
}

}
