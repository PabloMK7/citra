// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/loader/loader.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Loader namespace

namespace Loader {

/// Loads an ELF/AXF file
class AppLoader_ELF final : public AppLoader {
public:
    AppLoader_ELF(const std::string& filename);
    ~AppLoader_ELF() override;

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
