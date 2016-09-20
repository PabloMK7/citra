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
    Binary = 3,
};
constexpr size_t NUM_DSP_PIPE = 8;

/**
 * Reads `length` bytes from the DSP pipe identified with `pipe_number`.
 * @note Can read up to the maximum value of a u16 in bytes (65,535).
 * @note IF an error is encoutered with either an invalid `pipe_number` or `length` value, an empty
 * vector will be returned.
 * @note IF `length` is set to 0, an empty vector will be returned.
 * @note IF `length` is greater than the amount of data available, this function will only read the
 * available amount.
 * @param pipe_number a `DspPipe`
 * @param length the number of bytes to read. The max is 65,535 (max of u16).
 * @returns a vector of bytes from the specified pipe. On error, will be empty.
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
    Sleeping,
};

/// Get the state of the DSP
DspState GetDspState();

} // namespace HLE
} // namespace DSP
