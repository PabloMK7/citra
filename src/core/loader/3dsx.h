// Copyright 2014 Dolphin Emulator Project / Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/loader/loader.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Loader namespace

namespace Loader {

/// Loads an 3DSX file
class AppLoader_THREEDSX final : public AppLoader {
public:
    AppLoader_THREEDSX(const std::string& filename);
    ~AppLoader_THREEDSX() override;

    /**
     * Load the bootable file
     * @return ResultStatus result of function
     */
    ResultStatus Load() override;

private:
    std::string filename;
    bool        is_loaded = false;
};

} // namespace Loader
