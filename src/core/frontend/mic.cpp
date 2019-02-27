// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/frontend/mic.h"
#include "core/hle/service/mic_u.h"

namespace Frontend {

static std::shared_ptr<Mic::Interface> current_mic;

void RegisterMic(std::shared_ptr<Mic::Interface> mic) {
    current_mic = mic;
}

std::shared_ptr<Mic::Interface> GetCurrentMic() {
    if (!current_mic) {
        current_mic = std::make_shared<Mic::NullMic>();
    }
    return current_mic;
}

} // namespace Frontend
