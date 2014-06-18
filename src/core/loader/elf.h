// Copyright 2013 Dolphin Emulator Project / Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/loader/loader.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Loader namespace

namespace Loader {

/// Loads an ELF/AXF file
class AppLoader_ELF : public AppLoader {
public:
    AppLoader_ELF(std::string& filename);
    ~AppLoader_ELF();

    /**
     * Load the bootable file
     * @return ResultStatus result of function
     */
    const ResultStatus Load();

private:
    std::string filename;
    bool        is_loaded;
};

} // namespace Loader
