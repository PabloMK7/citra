// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <sys/stat.h>

#include "common/common_types.h"
#include "common/file_util.h"

#include "core/file_sys/file_sdmc.h"
#include "core/file_sys/archive_sdmc.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

File_SDMC::File_SDMC(const Archive_SDMC* archive, const std::string& path, const Mode mode) {
    // TODO(Link Mauve): normalize path into an absolute path without "..", it can currently bypass
    // the root directory we set while opening the archive.
    // For example, opening /../../etc/passwd can give the emulated program your users list.
    std::string real_path = archive->GetMountPoint() + path;

    if (!mode.create_flag && !FileUtil::Exists(real_path)) {
        file = nullptr;
        return;
    }

    std::string mode_string;
    if (mode.read_flag)
        mode_string += "r";
    if (mode.write_flag)
        mode_string += "w";

    file = new FileUtil::IOFile(real_path, mode_string.c_str());
}

File_SDMC::~File_SDMC() {
    Close();
}

size_t File_SDMC::Read(const u64 offset, const u32 length, u8* buffer) const {
    file->Seek(offset, SEEK_SET);
    return file->ReadBytes(buffer, length);
}

size_t File_SDMC::Write(const u64 offset, const u32 length, const u32 flush, const u8* buffer) const {
    file->Seek(offset, SEEK_SET);
    size_t written = file->WriteBytes(buffer, length);
    if (flush)
        file->Flush();
    return written;
}

size_t File_SDMC::GetSize() const {
    return file->GetSize();
}

bool File_SDMC::Close() const {
    return file->Close();
}

} // namespace FileSys
