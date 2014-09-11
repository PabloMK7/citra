// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "common/common_types.h"
#include "common/bit_field.h"

#include "core/file_sys/file.h"
#include "core/file_sys/directory.h"

#include "core/hle/kernel/kernel.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

union Mode {
    u32 hex;
    BitField<0, 1, u32> read_flag;
    BitField<1, 1, u32> write_flag;
    BitField<2, 1, u32> create_flag;
};

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
     * Open a file specified by its path, using the specified mode
     * @param path Path relative to the archive
     * @param mode Mode to open the file with
     * @return Opened file, or nullptr
     */
    virtual std::unique_ptr<File> OpenFile(const std::string& path, const Mode mode) const = 0;

    /**
     * Open a directory specified by its path
     * @param path Path relative to the archive
     * @return Opened directory, or nullptr
     */
    virtual std::unique_ptr<Directory> OpenDirectory(const std::string& path) const = 0;

    /**
     * Read data from the archive
     * @param offset Offset in bytes to start reading data from
     * @param length Length in bytes of data to read from archive
     * @param buffer Buffer to read data into
     * @return Number of bytes read
     */
    virtual size_t Read(const u64 offset, const u32 length, u8* buffer) const = 0;

    /**
     * Write data to the archive
     * @param offset Offset in bytes to start writing data to
     * @param length Length in bytes of data to write to archive
     * @param buffer Buffer to write data from
     * @param flush  The flush parameters (0 == do not flush)
     * @return Number of bytes written
     */
    virtual size_t Write(const u64 offset, const u32 length, const u32 flush, u8* buffer) = 0;

    /**
     * Get the size of the archive in bytes
     * @return Size of the archive in bytes
     */
    virtual size_t GetSize() const = 0;
    
    /**
     * Set the size of the archive in bytes
     */
    virtual void SetSize(const u64 size) = 0;
};

} // namespace FileSys
