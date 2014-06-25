// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#include "core/hle/kernel/kernel.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Kernel namespace

namespace Kernel {

/**
 * Creates an archive
 * @param name Optional name of archive
 * @return Handle to newly created archive object
 */
Handle CreateArchive(const std::string& name="Unknown");

} // namespace FileSys
