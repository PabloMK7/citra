// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _ATOMIC_H_
#define _ATOMIC_H_

#ifdef _WIN32

#include "atomic_win32.h"

#else

// GCC-compatible compiler assumed!
#include "atomic_gcc.h"

#endif

#endif
