// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/common.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Loader {

/**
 * Loads an NCCH file (e.g. from a CCI or CXI)
 * @param filename String filename of NCCH file
 * @param error_string Pointer to string to put error message if an error has occurred
 * @return True on success, otherwise false
 */
bool Load_NCCH(std::string& filename, std::string* error_string);

} // namespace Loader
