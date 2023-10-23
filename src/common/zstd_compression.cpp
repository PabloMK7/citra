// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <zstd.h>

#include "common/logging/log.h"
#include "common/zstd_compression.h"

namespace Common::Compression {

std::vector<u8> CompressDataZSTD(std::span<const u8> source, s32 compression_level) {
    compression_level = std::clamp(compression_level, ZSTD_minCLevel(), ZSTD_maxCLevel());
    const std::size_t max_compressed_size = ZSTD_compressBound(source.size());

    if (ZSTD_isError(max_compressed_size)) {
        LOG_ERROR(Common, "Error determining ZSTD maximum compressed size: {} ({})",
                  ZSTD_getErrorName(max_compressed_size), max_compressed_size);
        return {};
    }

    std::vector<u8> compressed(max_compressed_size);
    const std::size_t compressed_size = ZSTD_compress(
        compressed.data(), compressed.size(), source.data(), source.size(), compression_level);

    if (ZSTD_isError(compressed_size)) {
        LOG_ERROR(Common, "Error compressing ZSTD data: {} ({})",
                  ZSTD_getErrorName(compressed_size), compressed_size);
        return {};
    }

    compressed.resize(compressed_size);
    return compressed;
}

std::vector<u8> CompressDataZSTDDefault(std::span<const u8> source) {
    return CompressDataZSTD(source, ZSTD_CLEVEL_DEFAULT);
}

std::vector<u8> DecompressDataZSTD(std::span<const u8> compressed) {
    const std::size_t decompressed_size =
        ZSTD_getFrameContentSize(compressed.data(), compressed.size());

    if (decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        LOG_ERROR(Common, "ZSTD decompressed size could not be determined.");
        return {};
    }
    if (decompressed_size == ZSTD_CONTENTSIZE_ERROR || ZSTD_isError(decompressed_size)) {
        LOG_ERROR(Common, "Error determining ZSTD decompressed size: {} ({})",
                  ZSTD_getErrorName(decompressed_size), decompressed_size);
        return {};
    }

    std::vector<u8> decompressed(decompressed_size);
    const std::size_t uncompressed_result_size = ZSTD_decompress(
        decompressed.data(), decompressed.size(), compressed.data(), compressed.size());

    if (decompressed_size != uncompressed_result_size) {
        LOG_ERROR(Common, "ZSTD decompression expected {} bytes, got {}", decompressed_size,
                  uncompressed_result_size);
        return {};
    }
    if (ZSTD_isError(uncompressed_result_size)) {
        LOG_ERROR(Common, "Error decompressing ZSTD data: {} ({})",
                  ZSTD_getErrorName(uncompressed_result_size), uncompressed_result_size);
        return {};
    }

    return decompressed;
}

} // namespace Common::Compression
