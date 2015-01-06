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
    AppLoader_ELF(std::unique_ptr<FileUtil::IOFile>&& file) : AppLoader(std::move(file)) { }

    /**
     * Load the bootable file
     * @return ResultStatus result of function
     */
    ResultStatus Load() override;
};

} // namespace Loader
