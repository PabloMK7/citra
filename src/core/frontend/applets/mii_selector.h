// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <string>
#include "core/hle/applets/mii_selector.h"

namespace Frontend {

/// Configuration that's relevant to frontend implementation of applet. Anything missing that we
/// later learn is needed can be added here and filled in by the backend HLE applet
struct MiiSelectorConfig {
    bool enable_cancel_button;
    std::u16string title;
    u32 initially_selected_mii_index;
};

struct MiiSelectorData {
    u32 return_code;
    HLE::Applets::MiiData mii;
};

class MiiSelector {
public:
    virtual void Setup(const MiiSelectorConfig* config) {
        this->config = MiiSelectorConfig(*config);
    }
    const MiiSelectorData* ReceiveData() {
        return &data;
    }

    /**
     * Stores the data so that the HLE applet in core can
     * send this to the calling application
     */
    void Finalize(u32 return_code, HLE::Applets::MiiData mii);

protected:
    MiiSelectorConfig config;
    MiiSelectorData data;
};

class DefaultMiiSelector final : public MiiSelector {
public:
    void Setup(const MiiSelectorConfig* config) override;
};

void RegisterMiiSelector(std::shared_ptr<MiiSelector> applet);

std::shared_ptr<MiiSelector> GetRegisteredMiiSelector();

} // namespace Frontend
