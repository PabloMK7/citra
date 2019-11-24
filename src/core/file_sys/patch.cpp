// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <string>
#include "common/logging/log.h"
#include "core/file_sys/patch.h"

namespace FileSys::Patch {

bool ApplyIpsPatch(const std::vector<u8>& ips, std::vector<u8>& buffer) {
    u32 cursor = 5;
    u32 patch_length = ips.size() - 3;
    std::string ips_header(ips.begin(), ips.begin() + 5);

    if (ips_header != "PATCH") {
        LOG_INFO(Service_FS, "Attempted to load invalid IPS");
        return false;
    }

    while (cursor < patch_length) {
        std::string eof_check(ips.begin() + cursor, ips.begin() + cursor + 3);

        if (eof_check == "EOF")
            return false;

        u32 offset = ips[cursor] << 16 | ips[cursor + 1] << 8 | ips[cursor + 2];
        std::size_t length = ips[cursor + 3] << 8 | ips[cursor + 4];

        // check for an rle record
        if (length == 0) {
            length = ips[cursor + 5] << 8 | ips[cursor + 6];

            if (buffer.size() < offset + length)
                return false;

            for (u32 i = 0; i < length; ++i)
                buffer[offset + i] = ips[cursor + 7];

            cursor += 8;

            continue;
        }

        if (buffer.size() < offset + length)
            return false;

        std::memcpy(&buffer[offset], &ips[cursor + 5], length);
        cursor += length + 5;
    }
    return true;
}

} // namespace FileSys::Patch
