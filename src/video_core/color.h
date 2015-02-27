// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Color {

/// Convert a 1-bit color component to 8 bit
static inline u8 Convert1To8(u8 value) {
    return value * 255;
}

/// Convert a 4-bit color component to 8 bit
static inline u8 Convert4To8(u8 value) {
    return (value << 4) | value;
}

/// Convert a 5-bit color component to 8 bit
static inline u8 Convert5To8(u8 value) {
    return (value << 3) | (value >> 2);
}

/// Convert a 6-bit color component to 8 bit
static inline u8 Convert6To8(u8 value) {
    return (value << 2) | (value >> 4);
}

/// Convert a 8-bit color component to 1 bit
static inline u8 Convert8To1(u8 value) {
    return value >> 7;
}

/// Convert a 8-bit color component to 4 bit
static inline u8 Convert8To4(u8 value) {
    return value >> 4;
}

/// Convert a 8-bit color component to 5 bit
static inline u8 Convert8To5(u8 value) {
    return value >> 3;
}

/// Convert a 8-bit color component to 6 bit
static inline u8 Convert8To6(u8 value) {
    return value >> 2;
}

} // namespace
