// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <string>

#include "common/common_types.h"

namespace GameInfo {
std::vector<u8> GetSMDHData(std::string physical_name);

std::u16string GetTitle(std::string physical_name);

std::u16string GetPublisher(std::string physical_name);

std::string GetRegions(std::string physical_name);

std::vector<u16> GetIcon(std::string physical_name);
} // namespace GameInfo
