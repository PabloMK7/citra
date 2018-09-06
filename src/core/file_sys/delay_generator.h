// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace FileSys {

class DelayGenerator {
public:
    virtual u64 GetReadDelayNs(std::size_t length) = 0;

    // TODO (B3N30): Add getter for all other file/directory io operations
};

class DefaultDelayGenerator : public DelayGenerator {
public:
    u64 GetReadDelayNs(std::size_t length) override {
        // This is the delay measured for a romfs read.
        // For now we will take that as a default
        static constexpr u64 slope(94);
        static constexpr u64 offset(582778);
        static constexpr u64 minimum(663124);
        u64 IPCDelayNanoseconds = std::max<u64>(static_cast<u64>(length) * slope + offset, minimum);
        return IPCDelayNanoseconds;
    }
};

} // namespace FileSys
