// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>
#include "common/common_types.h"

namespace RomFS {

/**
 * Gets the pointer to a file in a RomFS image.
 * @param romfs The pointer to the RomFS image
 * @param path A vector containing the directory names and file name of the path to the file
 * @return the pointer to the file
 * @todo reimplement this with a full RomFS manager
 */
const u8* GetFilePointer(const u8* romfs, const std::vector<std::u16string>& path);

} // namespace RomFS
