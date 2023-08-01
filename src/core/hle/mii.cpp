// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/crc.hpp>
#include "core/hle/mii.h"

namespace Mii {
u16 ChecksummedMiiData::CalculateChecksum() {
    // Calculate the checksum of the selected Mii, see https://www.3dbrew.org/wiki/Mii#Checksum
    return boost::crc<16, 0x1021, 0, 0, false, false>(this, offsetof(ChecksummedMiiData, crc16));
}
} // namespace Mii
