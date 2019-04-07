// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/frontend/applets/mii_selector.h"

namespace Frontend {

void MiiSelector::Finalize(u32 return_code, HLE::Applets::MiiData mii) {
    data = {return_code, mii};
}

void DefaultMiiSelector::Setup(const Frontend::MiiSelectorConfig& config) {
    MiiSelector::Setup(config);
    Finalize(0, HLE::Applets::MiiSelector::GetStandardMiiResult().selected_mii_data);
}

} // namespace Frontend
