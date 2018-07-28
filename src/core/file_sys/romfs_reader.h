#pragma once

#include <array>
#include "common/common_types.h"
#include "common/file_util.h"

namespace FileSys {

class RomFSReader {
public:
    RomFSReader(FileUtil::IOFile&& file, std::size_t file_offset, std::size_t data_size)
        : is_encrypted(false), file(std::move(file)), file_offset(file_offset),
          data_size(data_size) {}

    RomFSReader(FileUtil::IOFile&& file, std::size_t file_offset, std::size_t data_size,
                const std::array<u8, 16>& key, const std::array<u8, 16>& ctr,
                std::size_t crypto_offset)
        : is_encrypted(true), file(std::move(file)), key(key), ctr(ctr), file_offset(file_offset),
          crypto_offset(crypto_offset), data_size(data_size) {}

    std::size_t GetSize() const {
        return data_size;
    }

    std::size_t ReadFile(std::size_t offset, std::size_t length, u8* buffer);

private:
    bool is_encrypted;
    FileUtil::IOFile file;
    std::array<u8, 16> key;
    std::array<u8, 16> ctr;
    std::size_t file_offset;
    std::size_t crypto_offset;
    std::size_t data_size;
};

} // namespace FileSys
