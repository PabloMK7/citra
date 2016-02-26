// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "common/common_types.h"

namespace DSP {
namespace HLE {

/// Reset the pipes by setting pipe positions back to the beginning.
void ResetPipes();

/**
 * Read a DSP pipe.
 * Pipe IDs:
 *   pipe_number = 0: Debug
 *   pipe_number = 1: P-DMA
 *   pipe_number = 2: Audio
 *   pipe_number = 3: Binary
 * @param pipe_number The Pipe ID
 * @param length How much data to request.
 * @return The data read from the pipe. The size of this vector can be less than the length requested.
 */
std::vector<u8> PipeRead(u32 pipe_number, u32 length);

/**
 * Write to a DSP pipe.
 * @param pipe_number The Pipe ID
 * @param buffer The data to write to the pipe.
 */
void PipeWrite(u32 pipe_number, const std::vector<u8>& buffer);

} // namespace HLE
} // namespace DSP
