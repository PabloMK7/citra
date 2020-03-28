#include <algorithm>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include "common/archives.h"
#include "core/file_sys/romfs_reader.h"

SERIALIZE_EXPORT_IMPL(FileSys::DirectRomFSReader)

namespace FileSys {

std::size_t DirectRomFSReader::ReadFile(std::size_t offset, std::size_t length, u8* buffer) {
    if (length == 0)
        return 0; // Crypto++ does not like zero size buffer
    file.Seek(file_offset + offset, SEEK_SET);
    std::size_t read_length = std::min(length, static_cast<std::size_t>(data_size) - offset);
    read_length = file.ReadBytes(buffer, read_length);
    if (is_encrypted) {
        CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption d(key.data(), key.size(), ctr.data());
        d.Seek(crypto_offset + offset);
        d.ProcessData(buffer, buffer, read_length);
    }
    return read_length;
}

} // namespace FileSys
