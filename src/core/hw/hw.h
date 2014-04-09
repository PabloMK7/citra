// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace HW {

template <typename T>
inline void Read(T &var, const u32 addr);

template <typename T>
inline void Write(u32 addr, const T data);

/// Update hardware
void Update();

/// Initialize hardware
void Init();

/// Shutdown hardware
void Shutdown();

} // namespace
