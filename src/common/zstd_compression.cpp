// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <zstd.h>

#include "common/assert.h"
#include "common/zstd_compression.h"

namespace Common::Compression {

std::vector<u8> CompressDataZSTD(const u8* source, std::size_t source_size, s32 compression_level) {
    compression_level = std::clamp(compression_level, ZSTD_minCLevel(), ZSTD_maxCLevel());

    const std::size_t max_compressed_size = ZSTD_compressBound(source_size);
    std::vector<u8> compressed(max_compressed_size);

    const std::size_t compressed_size =
        ZSTD_compress(compressed.data(), compressed.size(), source, source_size, compression_level);

    if (ZSTD_isError(compressed_size)) {
        // Compression failed
        return {};
    }

    compressed.resize(compressed_size);

    return compressed;
}

std::vector<u8> CompressDataZSTDDefault(const u8* source, std::size_t source_size) {
    return CompressDataZSTD(source, source_size, ZSTD_CLEVEL_DEFAULT);
}

std::vector<u8> DecompressDataZSTD(const std::vector<u8>& compressed) {
    const std::size_t decompressed_size =
        ZSTD_getDecompressedSize(compressed.data(), compressed.size());
    std::vector<u8> decompressed(decompressed_size);

    const std::size_t uncompressed_result_size = ZSTD_decompress(
        decompressed.data(), decompressed.size(), compressed.data(), compressed.size());

    if (decompressed_size != uncompressed_result_size || ZSTD_isError(uncompressed_result_size)) {
        // Decompression failed
        return {};
    }
    return decompressed;
}

} // namespace Common::Compression
