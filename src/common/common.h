// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

// DO NOT EVER INCLUDE <windows.h> directly _or indirectly_ from this file
// since it slows down the build a lot.

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/common_types.h"
#include "common/common_funcs.h"
#include "common/common_paths.h"
#include "common/platform.h"

#include "swap.h"
