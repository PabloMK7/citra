// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <span>
#include <vector>

#include "common/common_types.h"

namespace Common::Compression {

/**
 * Compresses a source memory region with Zstandard and returns the compressed data in a vector.
 *
 * @param source the uncompressed source memory region.
 * @param compression_level the used compression level. Should be between 1 and 22.
 *
 * @return the compressed data.
 */
[[nodiscard]] std::vector<u8> CompressDataZSTD(std::span<const u8> source, s32 compression_level);

/**
 * Compresses a source memory region with Zstandard with the default compression level and returns
 * the compressed data in a vector.
 *
 * @param source the uncompressed source memory region.
 *
 * @return the compressed data.
 */
[[nodiscard]] std::vector<u8> CompressDataZSTDDefault(std::span<const u8> source);

/**
 * Decompresses a source memory region with Zstandard and returns the uncompressed data in a vector.
 *
 * @param compressed the compressed source memory region.
 *
 * @return the decompressed data.
 */
[[nodiscard]] std::vector<u8> DecompressDataZSTD(std::span<const u8> compressed);

} // namespace Common::Compression
