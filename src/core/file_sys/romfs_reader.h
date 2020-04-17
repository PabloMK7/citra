#pragma once

#include <array>
#include <boost/serialization/array.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include "common/common_types.h"
#include "common/file_util.h"

namespace FileSys {

/**
 * Interface for reading RomFS data.
 */
class RomFSReader {
public:
    virtual ~RomFSReader() = default;

    virtual std::size_t GetSize() const = 0;
    virtual std::size_t ReadFile(std::size_t offset, std::size_t length, u8* buffer) = 0;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int file_version) {}
    friend class boost::serialization::access;
};

/**
 * A RomFS reader that directly reads the RomFS file.
 */
class DirectRomFSReader : public RomFSReader {
public:
    DirectRomFSReader(FileUtil::IOFile&& file, std::size_t file_offset, std::size_t data_size)
        : is_encrypted(false), file(std::move(file)), file_offset(file_offset),
          data_size(data_size) {}

    DirectRomFSReader(FileUtil::IOFile&& file, std::size_t file_offset, std::size_t data_size,
                      const std::array<u8, 16>& key, const std::array<u8, 16>& ctr,
                      std::size_t crypto_offset)
        : is_encrypted(true), file(std::move(file)), key(key), ctr(ctr), file_offset(file_offset),
          crypto_offset(crypto_offset), data_size(data_size) {}

    ~DirectRomFSReader() override = default;

    std::size_t GetSize() const override {
        return data_size;
    }

    std::size_t ReadFile(std::size_t offset, std::size_t length, u8* buffer) override;

private:
    bool is_encrypted;
    FileUtil::IOFile file;
    std::array<u8, 16> key;
    std::array<u8, 16> ctr;
    u64 file_offset;
    u64 crypto_offset;
    u64 data_size;

    DirectRomFSReader() = default;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<RomFSReader>(*this);
        ar& is_encrypted;
        ar& file;
        ar& key;
        ar& ctr;
        ar& file_offset;
        ar& crypto_offset;
        ar& data_size;
    }
    friend class boost::serialization::access;
};

} // namespace FileSys

BOOST_CLASS_EXPORT_KEY(FileSys::DirectRomFSReader)
