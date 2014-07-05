// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#include "core/hle/kernel/kernel.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

class Archive : NonCopyable {
public:
    /// Supported archive types
    enum class IdCode : u32 {
        RomFS               = 0x00000003,
        SaveData            = 0x00000004,
        ExtSaveData         = 0x00000006,
        SharedExtSaveData   = 0x00000007,
        SystemSaveData      = 0x00000008,
        SDMC                = 0x00000009,
        SDMCWriteOnly       = 0x0000000A,
    };

    Archive() { }
    virtual ~Archive() { }

    /**
     * Get the IdCode of the archive (e.g. RomFS, SaveData, etc.)
     * @return IdCode of the archive
     */
    virtual IdCode GetIdCode() const = 0;

    /**
     * Read data from the archive
     * @param offset Offset in bytes to start reading archive from
     * @param length Length in bytes to read data from archive
     * @param buffer Buffer to read data into
     * @return Number of bytes read
     */
    virtual size_t Read(const u64 offset, const u32 length, u8* buffer) const = 0;

    /**
     * Get the size of the archive in bytes
     * @return Size of the archive in bytes
     */
    virtual size_t GetSize() const = 0;
};

} // namespace FileSys
