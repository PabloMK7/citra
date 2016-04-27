// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <vector>

#include "common/common_types.h"

namespace DSP {
namespace HLE {

/// Reset the pipes by setting pipe positions back to the beginning.
void ResetPipes();

enum class DspPipe {
    Debug = 0,
    Dma = 1,
    Audio = 2,
    Binary = 3
};
constexpr size_t NUM_DSP_PIPE = 8;

/**
 * Read a DSP pipe.
 * @param pipe_number The Pipe ID
 * @param length How much data to request.
 * @return The data read from the pipe. The size of this vector can be less than the length requested.
 */
std::vector<u8> PipeRead(DspPipe pipe_number, u32 length);

/**
 * How much data is left in pipe
 * @param pipe_number The Pipe ID
 * @return The amount of data remaning in the pipe. This is the maximum length PipeRead will return.
 */
size_t GetPipeReadableSize(DspPipe pipe_number);

/**
 * Write to a DSP pipe.
 * @param pipe_number The Pipe ID
 * @param buffer The data to write to the pipe.
 */
void PipeWrite(DspPipe pipe_number, const std::vector<u8>& buffer);

enum class DspState {
    Off,
    On,
    Sleeping
};
/// Get the state of the DSP
DspState GetDspState();

} // namespace HLE
} // namespace DSP
