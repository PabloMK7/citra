// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include "core/file_sys/archive_source_sd_savedata.h"

namespace FileSys {

/// File system interface to the OtherSaveDataPermitted archive
class ArchiveFactory_OtherSaveDataPermitted final : public ArchiveFactory {
public:
    explicit ArchiveFactory_OtherSaveDataPermitted(
        std::shared_ptr<ArchiveSource_SDSaveData> sd_savedata_source);

    std::string GetName() const override {
        return "OtherSaveDataPermitted";
    }

    ResultVal<std::unique_ptr<ArchiveBackend>> Open(const Path& path, u64 program_id) override;
    Result Format(const Path& path, const FileSys::ArchiveFormatInfo& format_info, u64 program_id,
                  u32 directory_buckets, u32 file_buckets) override;
    ResultVal<ArchiveFormatInfo> GetFormatInfo(const Path& path, u64 program_id) const override;

    bool IsSlow() override {
        return sd_savedata_source->IsUsingArtic();
    }

private:
    std::shared_ptr<ArchiveSource_SDSaveData> sd_savedata_source;

    ArchiveFactory_OtherSaveDataPermitted() = default;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<ArchiveFactory>(*this);
        ar& sd_savedata_source;
    }
    friend class boost::serialization::access;
};

/// File system interface to the OtherSaveDataGeneral archive
class ArchiveFactory_OtherSaveDataGeneral final : public ArchiveFactory {
public:
    explicit ArchiveFactory_OtherSaveDataGeneral(
        std::shared_ptr<ArchiveSource_SDSaveData> sd_savedata_source);

    std::string GetName() const override {
        return "OtherSaveDataGeneral";
    }

    ResultVal<std::unique_ptr<ArchiveBackend>> Open(const Path& path, u64 program_id) override;
    Result Format(const Path& path, const FileSys::ArchiveFormatInfo& format_info, u64 program_id,
                  u32 directory_buckets, u32 file_buckets) override;
    ResultVal<ArchiveFormatInfo> GetFormatInfo(const Path& path, u64 program_id) const override;

private:
    std::shared_ptr<ArchiveSource_SDSaveData> sd_savedata_source;

    ArchiveFactory_OtherSaveDataGeneral() = default;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<ArchiveFactory>(*this);
        ar& sd_savedata_source;
    }
    friend class boost::serialization::access;
};

} // namespace FileSys

BOOST_CLASS_EXPORT_KEY(FileSys::ArchiveFactory_OtherSaveDataPermitted)
BOOST_CLASS_EXPORT_KEY(FileSys::ArchiveFactory_OtherSaveDataGeneral)
