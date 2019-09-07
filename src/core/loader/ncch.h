// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/file_sys/ncch_container.h"
#include "core/loader/loader.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Loader namespace

namespace Loader {

/// Loads an NCCH file (e.g. from a CCI, or the first NCCH in a CXI)
class AppLoader_NCCH final : public AppLoader {
public:
    AppLoader_NCCH(FileUtil::IOFile&& file, const std::string& filepath)
        : AppLoader(std::move(file)), base_ncch(filepath), overlay_ncch(&base_ncch),
          filepath(filepath) {}

    /**
     * Returns the type of the file
     * @param file FileUtil::IOFile open file
     * @return FileType found, or FileType::Error if this loader doesn't know it
     */
    static FileType IdentifyType(FileUtil::IOFile& file);

    FileType GetFileType() override {
        return IdentifyType(file);
    }

    ResultStatus Load(std::shared_ptr<Kernel::Process>& process) override;

    /**
     * Loads the Exheader and returns the system mode for this application.
     * @returns A pair with the optional system mode, and and the status.
     */
    std::pair<std::optional<u32>, ResultStatus> LoadKernelSystemMode() override;

    ResultStatus IsExecutable(bool& out_executable) override;

    ResultStatus ReadCode(std::vector<u8>& buffer) override;

    ResultStatus ReadIcon(std::vector<u8>& buffer) override;

    ResultStatus ReadBanner(std::vector<u8>& buffer) override;

    ResultStatus ReadLogo(std::vector<u8>& buffer) override;

    ResultStatus ReadProgramId(u64& out_program_id) override;

    ResultStatus ReadExtdataId(u64& out_extdata_id) override;

    ResultStatus ReadRomFS(std::shared_ptr<FileSys::RomFSReader>& romfs_file) override;

    ResultStatus ReadUpdateRomFS(std::shared_ptr<FileSys::RomFSReader>& romfs_file) override;

    ResultStatus ReadTitle(std::string& title) override;

private:
    /**
     * Loads .code section into memory for booting
     * @param process The newly created process
     * @return ResultStatus result of function
     */
    ResultStatus LoadExec(std::shared_ptr<Kernel::Process>& process);

    /// Reads the region lockout info in the SMDH and send it to CFG service
    void ParseRegionLockoutInfo();

    FileSys::NCCHContainer base_ncch;
    FileSys::NCCHContainer update_ncch;
    FileSys::NCCHContainer* overlay_ncch;

    std::string filepath;
};

} // namespace Loader
