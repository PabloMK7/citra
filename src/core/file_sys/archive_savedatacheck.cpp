// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/file_util.h"

#include "core/file_sys/archive_savedatacheck.h"
#include "core/hle/service/fs/archive.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

static std::string GetSaveDataCheckContainerPath(const std::string& mount_point) {
    return Common::StringFromFormat("%s%s/title/", mount_point.c_str(), SYSTEM_ID.c_str());
}

static std::string GetSaveDataCheckPath(const std::string& mount_point, u32 high, u32 low) {
    return Common::StringFromFormat("%s%08x/%08x/content/00000000.app.romfs",
            mount_point.c_str(), high, low);
}

Archive_SaveDataCheck::Archive_SaveDataCheck(const std::string& mount_loc) :
mount_point(GetSaveDataCheckContainerPath(mount_loc)) {
}

ResultCode Archive_SaveDataCheck::Open(const Path& path) {
    // TODO(Subv): We should not be overwriting raw_data everytime this function is called,
    // but until we use factory classes to create the archives at runtime instead of creating them beforehand
    // and allow multiple archives of the same type to be open at the same time without clobbering each other,
    // we won't be able to maintain the state of each archive, hence we overwrite it every time it's needed.
    // There are a number of problems with this, for example opening a file in this archive, then opening
    // this archive again with a different path, will corrupt the previously open file.
    auto vec = path.AsBinary();
    const u32* data = reinterpret_cast<u32*>(vec.data());
    std::string file_path = GetSaveDataCheckPath(mount_point, data[1], data[0]);
    FileUtil::IOFile file(file_path, "rb");

    std::fill(raw_data.begin(), raw_data.end(), 0);

    if (!file.IsOpen()) {
        return ResultCode(-1); // TODO(Subv): Find the right error code
    }
    auto size = file.GetSize();
    raw_data.resize(size);
    file.ReadBytes(raw_data.data(), size);
    file.Close();
    return RESULT_SUCCESS;
}

} // namespace FileSys
