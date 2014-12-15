// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#include "core/file_sys/directory_backend.h"
#include "core/loader/loader.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

class Directory_RomFS final : public DirectoryBackend {
public:
    Directory_RomFS();
    ~Directory_RomFS() override;

    /**
    * Open the directory
    * @return true if the directory opened correctly
    */
    bool Open() override;

    /**
     * List files contained in the directory
     * @param count Number of entries to return at once in entries
     * @param entries Buffer to read data into
     * @return Number of entries listed
     */
    u32 Read(const u32 count, Entry* entries) override;

    /**
     * Close the directory
     * @return true if the directory closed correctly
     */
    bool Close() const override;
};

} // namespace FileSys
